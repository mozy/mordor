// Copyright (c) 2009 - Mozy, Inc.

#include "fiber.h"

#ifdef HAVE_CONFIG_H
#include "autoconfig.h"
#endif

#ifdef HAVE_VALGRIND_VALGRIND_H
#include <valgrind/valgrind.h>
#endif

#include <boost/thread/tss.hpp>

#include "assert.h"
#include "config.h"
#include "exception.h"
#include "statistics.h"
#include "version.h"

#ifdef WINDOWS
#include <windows.h>

#include "runtime_linking.h"
#else
#include <sys/mman.h>
#include <pthread.h>
#endif

namespace Mordor {

static AverageMinMaxStatistic<unsigned int> &g_statAlloc =
    Statistics::registerStatistic("fiber.allocstack",
    AverageMinMaxStatistic<unsigned int>("us"));
static AverageMinMaxStatistic<unsigned int> &g_statFree=
    Statistics::registerStatistic("fiber.freestack",
    AverageMinMaxStatistic<unsigned int>("us"));

static void fiber_switchContext(void **oldsp, void *newsp);

#ifdef SETJMP_FIBERS
#ifdef OSX
#define setjmp _setjmp
#define longjmp _longjmp
#endif
#endif

static size_t g_pagesize;

namespace {

static struct FiberInitializer {
    FiberInitializer()
    {
#ifdef WINDOWS
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        g_pagesize = info.dwPageSize;
#elif defined(POSIX)
        g_pagesize = sysconf(_SC_PAGESIZE);
#endif
    }
} g_init;

}

static ConfigVar<size_t>::ptr g_defaultStackSize = Config::lookup<size_t>(
    "fiber.defaultstacksize",
#ifdef NATIVE_WINDOWS_FIBERS
    0u,
#else
    1024 * 1024u,
#endif
    "Default stack size for new fibers.  This is the virtual size; physical "
    "memory isn't consumed until it is actually referenced.");

// t_fiber is the Fiber currently executing on this thread
// t_threadFiber is the Fiber that represents the thread's original stack
// t_threadFiber is a boost::tss, because it supports automatic cleanup when
// the thread exits (and datatypes larger than pointer size), while
// ThreadLocalStorage does not
// t_fiber is a ThreadLocalStorage, because it's faster than boost::tss
ThreadLocalStorage<Fiber *> Fiber::t_fiber;
static boost::thread_specific_ptr<Fiber::ptr> t_threadFiber;

static boost::mutex & g_flsMutex()
{
    static boost::mutex mutex;
    return mutex;
}
static std::vector<bool> & g_flsIndices()
{
    static std::vector<bool> indices;
    return indices;
}

Fiber::Fiber()
{
    MORDOR_ASSERT(!t_fiber);
    m_state = EXEC;
    m_stack = NULL;
    m_stacksize = 0;
    m_sp = NULL;
    setThis(this);
#ifdef NATIVE_WINDOWS_FIBERS
    if (!pIsThreadAFiber())
        m_stack = ConvertThreadToFiber(NULL);
    m_sp = GetCurrentFiber();
#elif defined(UCONTEXT_FIBERS)
    m_sp = &m_ctx;
#elif defined(SETJMP_FIBERS)
    m_sp = &m_env;
#endif
}

Fiber::Fiber(boost::function<void ()> dg, size_t stacksize)
{
    stacksize += g_pagesize - 1;
    stacksize -= stacksize % g_pagesize;
    m_dg = dg;
    m_state = INIT;
    m_stack = NULL;
    m_stacksize = stacksize;
    allocStack();
#ifdef UCONTEXT_FIBERS
    m_sp = &m_ctx;
#elif defined(SETJMP_FIBERS)
    m_sp = &m_env;
#endif
    initStack();
}

Fiber::~Fiber()
{
    if (!m_stack || m_stack == m_sp) {
        // Thread entry fiber
        MORDOR_ASSERT(!m_dg);
        MORDOR_ASSERT(m_state == EXEC);
        Fiber *cur = t_fiber.get();

        // We're actually running on the fiber we're about to delete
        // i.e. the thread is dying, so clean up after ourselves
        if (cur == this)  {
            setThis(NULL);
#ifdef NATIVE_WINDOWS_FIBERS
            if (m_stack) {
                MORDOR_ASSERT(m_stack == m_sp);
                MORDOR_ASSERT(m_stack == GetCurrentFiber());
                pConvertFiberToThread();
            }
#endif
        }
        // Otherwise, there's not a thread left to clean up
    } else {
        // Regular fiber
        MORDOR_ASSERT(m_state == TERM || m_state == INIT || m_state == EXCEPT);
        freeStack();
    }
}

void
Fiber::reset()
{
    m_exception = boost::exception_ptr();
    MORDOR_ASSERT(m_stack);
    MORDOR_ASSERT(m_state == TERM || m_state == INIT || m_state == EXCEPT);
    MORDOR_ASSERT(m_dg);
    initStack();
    m_state = INIT;
}

void
Fiber::reset(boost::function<void ()> dg)
{
    m_exception = boost::exception_ptr();
    MORDOR_ASSERT(m_stack);
    MORDOR_ASSERT(m_state == TERM || m_state == INIT || m_state == EXCEPT);
    m_dg = dg;
    initStack();
    m_state = INIT;
}

Fiber::ptr
Fiber::getThis()
{
    if (t_fiber)
        return t_fiber->shared_from_this();
    Fiber::ptr threadFiber(new Fiber());
    MORDOR_ASSERT(t_fiber.get() == threadFiber.get());
    t_threadFiber.reset(new Fiber::ptr(threadFiber));
    return t_fiber->shared_from_this();
}

void
Fiber::setThis(Fiber* f)
{
    t_fiber = f;
}

void
Fiber::call()
{
    MORDOR_ASSERT(!m_outer);
    ptr cur = getThis();
    MORDOR_ASSERT(m_state == HOLD || m_state == INIT);
    MORDOR_ASSERT(cur);
    MORDOR_ASSERT(cur.get() != this);
    setThis(this);
    m_outer = cur;
    m_state = m_exception ? EXCEPT : EXEC;
    fiber_switchContext(&cur->m_sp, m_sp);
    setThis(cur.get());
    MORDOR_ASSERT(cur->m_yielder);
    m_outer.reset();
    if (cur->m_yielder) {
        MORDOR_ASSERT(cur->m_yielder.get() == this);
        Fiber::ptr yielder = cur->m_yielder;
        yielder->m_state = cur->m_yielderNextState;
        cur->m_yielder.reset();
        if (yielder->m_state == EXCEPT && yielder->m_exception)
            Mordor::rethrow_exception(yielder->m_exception);
    }
    MORDOR_ASSERT(cur->m_state == EXEC);
}

void
Fiber::inject(boost::exception_ptr exception)
{
    MORDOR_ASSERT(exception);
    m_exception = exception;
    call();
}

Fiber::ptr
Fiber::yieldTo(bool yieldToCallerOnTerminate)
{
    return yieldTo(yieldToCallerOnTerminate, HOLD);
}

void
Fiber::yield()
{
    ptr cur = getThis();
    MORDOR_ASSERT(cur);
    MORDOR_ASSERT(cur->m_state == EXEC);
    MORDOR_ASSERT(cur->m_outer);
    cur->m_outer->m_yielder = cur;
    cur->m_outer->m_yielderNextState = Fiber::HOLD;
    fiber_switchContext(&cur->m_sp, cur->m_outer->m_sp);
    if (cur->m_yielder) {
        cur->m_yielder->m_state = cur->m_yielderNextState;
        cur->m_yielder.reset();
    }
    if (cur->m_state == EXCEPT) {
        MORDOR_ASSERT(cur->m_exception);
        Mordor::rethrow_exception(cur->m_exception);
    }
    MORDOR_ASSERT(cur->m_state == EXEC);
}

Fiber::State
Fiber::state()
{
    return m_state;
}

Fiber::ptr
Fiber::yieldTo(bool yieldToCallerOnTerminate, State targetState)
{
    MORDOR_ASSERT(m_state == HOLD || m_state == INIT);
    MORDOR_ASSERT(targetState == HOLD || targetState == TERM || targetState == EXCEPT);
    ptr cur = getThis();
    MORDOR_ASSERT(cur);
    setThis(this);
    if (yieldToCallerOnTerminate) {
        Fiber::ptr outer = shared_from_this();
        Fiber::ptr previous;
        while (outer) {
            previous = outer;
            outer = outer->m_outer;
        }
        previous->m_terminateOuter = cur;
    }
    m_state = EXEC;
    m_yielder = cur;
    m_yielderNextState = targetState;
    Fiber *curp = cur.get();
    // Relinguish our reference
    cur.reset();
    fiber_switchContext(&curp->m_sp, m_sp);
#ifdef NATIVE_WINDOWS_FIBERS
    if (targetState == TERM)
        return Fiber::ptr();
#endif
    MORDOR_ASSERT(targetState != TERM);
    setThis(curp);
    if (curp->m_yielder) {
        Fiber::ptr yielder = curp->m_yielder;
        yielder->m_state = curp->m_yielderNextState;
        curp->m_yielder.reset();
        if (yielder->m_exception)
            Mordor::rethrow_exception(yielder->m_exception);
        return yielder;
    }
    if (curp->m_state == EXCEPT) {
        MORDOR_ASSERT(curp->m_exception);
        Mordor::rethrow_exception(curp->m_exception);
    }
    MORDOR_ASSERT(curp->m_state == EXEC);
    return Fiber::ptr();
}

void
Fiber::entryPoint()
{
    // This function never returns, so take care that smart pointers (or other resources)
    // are properly released.
    ptr cur = getThis();
    MORDOR_ASSERT(cur);
    if (cur->m_yielder) {
        cur->m_yielder->m_state = cur->m_yielderNextState;
        cur->m_yielder.reset();
    }
    MORDOR_ASSERT(cur->m_dg);
    State nextState = TERM;
    try {
        if (cur->m_state == EXCEPT) {
            MORDOR_ASSERT(cur->m_exception);
            Mordor::rethrow_exception(cur->m_exception);
        }
        MORDOR_ASSERT(cur->m_state == EXEC);
        cur->m_dg();
    } catch (boost::exception &ex) {
        removeTopFrames(ex);
        cur->m_exception = boost::current_exception();
        nextState = EXCEPT;
    } catch (...) {
        cur->m_exception = boost::current_exception();
        nextState = EXCEPT;
    }

    exitPoint(cur, nextState);
#ifndef NATIVE_WINDOWS_FIBERS
    MORDOR_NOTREACHED();
#endif
}

void
Fiber::exitPoint(Fiber::ptr &cur, State targetState)
{
    // This function never returns, so take care that smart pointers (or other resources)
    // are properly released.
    Fiber::ptr outer;
    Fiber *rawPtr = NULL;
    if (!cur->m_terminateOuter.expired() && !cur->m_outer) {
        outer = cur->m_terminateOuter.lock();
        rawPtr = outer.get();
    } else {
        outer = cur->m_outer;
        rawPtr = cur.get();
    }
    MORDOR_ASSERT(outer);
    MORDOR_ASSERT(rawPtr);
    MORDOR_ASSERT(outer != cur);

    // Have to set this reference before calling yieldTo()
    // so we can reset cur before we call yieldTo()
    // (since it's not ever going to destruct)
    outer->m_yielder = cur;
    outer->m_yielderNextState = targetState;
    MORDOR_ASSERT(!cur.unique());
    cur.reset();
    if (rawPtr == outer.get()) {
        rawPtr = outer.get();
        MORDOR_ASSERT(!outer.unique());
        outer.reset();
        rawPtr->yieldTo(false, targetState);
    } else {
        outer.reset();
        fiber_switchContext(&rawPtr->m_sp, rawPtr->m_outer->m_sp);
    }
}

#ifdef NATIVE_WINDOWS_FIBERS
static VOID CALLBACK native_fiber_entryPoint(PVOID lpParameter)
{
    void (*entryPoint)() = (void (*)())lpParameter;
    while (true) {
        entryPoint();
    }
}
#endif

void
Fiber::allocStack()
{
    if (m_stacksize == 0)
        m_stacksize = g_defaultStackSize->val();
#ifndef NATIVE_WINDOWS_FIBERS
    TimeStatistic<AverageMinMaxStatistic<unsigned int> > time(g_statAlloc);
#endif
#ifdef NATIVE_WINDOWS_FIBERS
    // Fibers are allocated in initStack
#elif defined(WINDOWS)
    m_stack = VirtualAlloc(NULL, m_stacksize + g_pagesize, MEM_RESERVE, PAGE_NOACCESS);
    if (!m_stack)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("VirtualAlloc");
    VirtualAlloc(m_stack, g_pagesize, MEM_COMMIT, PAGE_READWRITE | PAGE_GUARD);
    // TODO: don't commit until referenced
    VirtualAlloc((char*)m_stack + g_pagesize, m_stacksize, MEM_COMMIT, PAGE_READWRITE);
    m_sp = (char*)m_stack + m_stacksize + g_pagesize;
#elif defined(POSIX)
    m_stack = mmap(NULL, m_stacksize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (m_stack == MAP_FAILED)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("mmap");
#ifdef HAVE_VALGRIND_VALGRIND_H
    m_valgrindStackId = VALGRIND_STACK_REGISTER(m_stack, (char *)m_stack + m_stacksize);
#endif
    m_sp = (char*)m_stack + m_stacksize;
#endif
}

void
Fiber::freeStack()
{
    TimeStatistic<AverageMinMaxStatistic<unsigned int> > time(g_statFree);
#ifdef NATIVE_WINDOWS_FIBERS
    MORDOR_ASSERT(m_stack == &m_sp);
    DeleteFiber(m_sp);
#elif defined(WINDOWS)
    VirtualFree(m_stack, 0, MEM_RELEASE);
#elif defined(POSIX)
#ifdef HAVE_VALGRIND_VALGRIND_H
    VALGRIND_STACK_DEREGISTER(m_valgrindStackId);
#endif
    munmap(m_stack, m_stacksize);
#endif
}

#ifdef NATIVE_WINDOWS_FIBERS
static void
fiber_switchContext(void **oldsp, void *newsp)
{
    SwitchToFiber(newsp);
}
#elif defined(UCONTEXT_FIBERS)
static void
fiber_switchContext(void **oldsp, void *newsp)
{
    if (swapcontext(*(ucontext_t**)oldsp, (ucontext_t*)newsp))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("swapcontext");
}
#elif defined(SETJMP_FIBERS)
static void
fiber_switchContext(void **oldsp, void *newsp)
{
    if (!setjmp(**(jmp_buf**)oldsp))
         longjmp(*(jmp_buf*)newsp, 1);
}
#endif


void
Fiber::initStack()
{
#ifdef NATIVE_WINDOWS_FIBERS
    if (m_stack)
        return;
    TimeStatistic<AverageMinMaxStatistic<unsigned int> > stat(g_statAlloc);
    m_sp = m_stack = pCreateFiberEx(0, m_stacksize, 0, &native_fiber_entryPoint, &Fiber::entryPoint);
    stat.finish();
    if (!m_stack)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateFiber");
    // This is so we can distinguish from a created fiber vs. the "root" fiber
    m_stack = &m_sp;
#elif defined(UCONTEXT_FIBERS)
    if (getcontext(&m_ctx))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("getcontext");
    m_ctx.uc_link = NULL;
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stacksize;
#ifdef OSX
    m_ctx.uc_mcsize = sizeof(m_mctx);
    m_ctx.uc_mcontext = (mcontext_t)m_mctx;
#endif
    makecontext(&m_ctx, &Fiber::entryPoint, 0);
#elif defined(SETJMP_FIBERS)
    if (setjmp(m_env)) {
        Fiber::entryPoint();
        MORDOR_NOTREACHED();
    }
#ifdef OSX
#ifdef X86
    m_env[8] = 0xffffffff; // EBP
    m_env[9] = (int)m_stack + m_stacksize; // ESP
#elif defined(X86_64)
    long long *env = (long long *)m_env;
    env[1] = 0x0ll; // RBP
    env[2] = (long long)m_stack + m_stacksize; // RSP
#elif defined(PPC)
    m_env[0] = (int)m_stack;
#else
#error Architecture not supported
#endif
#elif defined (LINUX)
#ifdef ARM
    int *env = (int *)m_env;
    env[8] = (int)m_stack + m_stacksize;
#else
#error Platform not supported
#endif
#else
#error Platform not supported
#endif
#endif
}

#ifdef WINDOWS
static bool g_doesntHaveOSFLS;
#endif

size_t
Fiber::flsAlloc()
{
#ifdef WINDOWS
    while (!g_doesntHaveOSFLS) {
        size_t result = pFlsAlloc(NULL);
        if (result == FLS_OUT_OF_INDEXES && lastError() == ERROR_CALL_NOT_IMPLEMENTED) {
            g_doesntHaveOSFLS = true;
            break;
        }
        if (result == FLS_OUT_OF_INDEXES)
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("FlsAlloc");
        return result;
    }
#endif
    boost::mutex::scoped_lock lock(g_flsMutex());
    std::vector<bool>::iterator it = std::find(g_flsIndices().begin(),
        g_flsIndices().end(), false);
    // TODO: we don't clear out values when freeing, so we can't reuse
    // force new
    it = g_flsIndices().end();
    if (it == g_flsIndices().end()) {
        g_flsIndices().resize(g_flsIndices().size() + 1);
        g_flsIndices()[g_flsIndices().size() - 1] = true;
        return g_flsIndices().size() - 1;
    } else {
        size_t result = it - g_flsIndices().begin();
        g_flsIndices()[result] = true;
        return result;
    }
}

void
Fiber::flsFree(size_t key)
{
#ifdef WINDOWS
    if (!g_doesntHaveOSFLS) {
        if (!pFlsFree((DWORD)key))
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("FlsFree");
        return;
    }
#endif
    boost::mutex::scoped_lock lock(g_flsMutex());
    MORDOR_ASSERT(key < g_flsIndices().size());
    MORDOR_ASSERT(g_flsIndices()[key]);
    if (key + 1 == g_flsIndices().size()) {
        g_flsIndices().resize(key);
    } else {
        // TODO: clear out current values
        g_flsIndices()[key] = false;
    }
}

void
Fiber::flsSet(size_t key, intptr_t value)
{
#ifdef WINDOWS
    if (!g_doesntHaveOSFLS) {
        if (!pFlsSetValue((DWORD)key, (PVOID)value))
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("FlsSetValue");
        return;
    }
#endif
    Fiber::ptr self = Fiber::getThis();
    if (self->m_fls.size() <= key)
        self->m_fls.resize(key + 1);
    self->m_fls[key] = value;
}

intptr_t
Fiber::flsGet(size_t key)
{
#ifdef WINDOWS
    if (!g_doesntHaveOSFLS) {
        error_t error = lastError();
        intptr_t result = (intptr_t)pFlsGetValue((DWORD)key);
        lastError(error);
        return result;
    }
#endif
    Fiber::ptr self = Fiber::getThis();
    if (self->m_fls.size() <= key)
        return 0;
    return self->m_fls[key];
}

std::vector<void *>
Fiber::backtrace()
{
    MORDOR_ASSERT(m_state != EXEC);
    std::vector<void *> result;
    if (m_state != HOLD)
        return result;
#ifdef WINDOWS
    STACKFRAME64 frame;
    DWORD type;
    CONTEXT *context;
#ifdef _M_IX86
    context = (CONTEXT *)((char *)m_sp + 0x14);
    type                   = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset    = context->Eip;
    frame.AddrPC.Mode      = AddrModeFlat;
    frame.AddrFrame.Offset = context->Ebp;
    frame.AddrFrame.Mode   = AddrModeFlat;
    frame.AddrStack.Offset = context->Esp;
    frame.AddrStack.Mode   = AddrModeFlat;
    context = NULL;
#elif _M_X64
    context = (CONTEXT *)((char *)m_sp + 0x30);
    CONTEXT dupContext;
    memcpy(&dupContext, context, sizeof(CONTEXT));
    context = &dupContext;
    type                   = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset    = dupContext.Rip;
    frame.AddrPC.Mode      = AddrModeFlat;
    frame.AddrFrame.Offset = dupContext.Rsp;
    frame.AddrFrame.Mode   = AddrModeFlat;
    frame.AddrStack.Offset = dupContext.Rsp;
    frame.AddrStack.Mode   = AddrModeFlat;
#else
#error "Unsupported platform"
#endif

    while (result.size() < 64) {
        if (!StackWalk64(type, GetCurrentProcess(), GetCurrentThread(),
            &frame, context, NULL, &SymFunctionTableAccess64,
            &SymGetModuleBase64, NULL)) {
            error_t error = lastError();
            break;
        }
        if (frame.AddrPC.Offset != 0) {
            result.push_back((void *)frame.AddrPC.Offset);
        }
    }
#endif
    return result;
}

std::ostream &operator<<(std::ostream &os, Fiber::State state)
{
    switch (state) {
        case Fiber::INIT:
            return os << "INIT";
        case Fiber::HOLD:
            return os << "HOLD";
        case Fiber::EXEC:
            return os << "EXEC";
        case Fiber::EXCEPT:
            return os << "EXCEPT";
        case Fiber::TERM:
            return os << "TERM";
        default:
            return os << (int)state;
    }
}

}
