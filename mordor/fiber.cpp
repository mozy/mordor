// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "fiber.h"

#include "assert.h"
#include "config.h"
#include "exception.h"
#include "version.h"

#ifdef WINDOWS
#include <windows.h>
#else
#include <sys/mman.h>
#include <pthread.h>
#endif

namespace Mordor {

#if defined(NATIVE_WINDOWS_FIBERS) || defined(UCONTEXT_FIBERS) || defined(SETJMP_FIBERS)
static
#else
static void push(void * &sp, size_t v);
extern "C"
#endif
void fiber_switchContext(void **oldsp, void *newsp);

#ifdef SETJMP_FIBERS
#ifdef OSX
#define setjmp _setjmp
#define longjmp _longjmp
#endif
#endif

static size_t g_pagesize;

namespace {

static struct Initializer {
    Initializer()
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

static void delete_nothing(Fiber* f) {}

boost::thread_specific_ptr<Fiber> Fiber::t_fiber(&delete_nothing);

Fiber::Fiber()
{
    MORDOR_ASSERT(!getThis());
    m_state = EXEC;
    m_stack = NULL;
    m_stacksize = 0;
    m_sp = NULL;
    setThis(this);
#ifdef NATIVE_WINDOWS_FIBERS
    m_sp = ConvertThreadToFiber(NULL);
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
    if (m_state == EXCEPT) {
        m_exception = boost::exception_ptr();
        call(true);
        m_state = TERM;
    }
    if (!m_stack) {
        MORDOR_ASSERT(!m_dg);
        MORDOR_ASSERT(m_state == EXEC);
#ifdef DEBUG
        Fiber *cur = t_fiber.get();
        MORDOR_ASSERT(cur);
        MORDOR_ASSERT(cur == this);
#endif
        setThis(NULL);
#ifdef NATIVE_WINDOWS_FIBERS
        ConvertFiberToThread();
#endif
    } else {
        MORDOR_ASSERT(m_state == TERM || m_state == INIT);
        freeStack();
    }
}

void
Fiber::reset()
{
    m_exception = boost::exception_ptr();
    if (m_state == EXCEPT)
        call(true);
    MORDOR_ASSERT(m_stack);
    MORDOR_ASSERT(m_state == TERM || m_state == INIT);
    MORDOR_ASSERT(m_dg);
    initStack();
    m_state = INIT;
}

void
Fiber::reset(boost::function<void ()> dg)
{
    m_exception = boost::exception_ptr();
    if (m_state == EXCEPT)
        call(true);
    MORDOR_ASSERT(m_stack);
    MORDOR_ASSERT(m_state == TERM || m_state == INIT);
    m_dg = dg;
    initStack();
    m_state = INIT;
}

Fiber::ptr
Fiber::getThis()
{
    if (t_fiber.get())
        return t_fiber->shared_from_this();
    return ptr();
}

void
Fiber::setThis(Fiber* f)
{
    t_fiber.reset(f);
}

void
Fiber::call()
{
    call(false);
}

void
Fiber::inject(boost::exception_ptr exception)
{
    MORDOR_ASSERT(exception);
    m_exception = exception;
    call(false);
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

void
Fiber::call(bool destructor)
{
    MORDOR_ASSERT(!m_outer);
    ptr cur = getThis();
    if (destructor) {
        MORDOR_ASSERT(m_state == EXCEPT);
    } else {
        MORDOR_ASSERT(m_state == HOLD || m_state == INIT);
        MORDOR_ASSERT(cur);
        MORDOR_ASSERT(cur.get() != this);
    }
    setThis(this);
    m_outer = cur;
    m_state = m_exception ? EXCEPT : EXEC;
    fiber_switchContext(&cur->m_sp, m_sp);
    setThis(cur.get());
    MORDOR_ASSERT(cur->m_yielder || destructor);
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
    ptr cur = getThis();
    MORDOR_ASSERT(cur);
    if (cur->m_yielder) {
        cur->m_yielder->m_state = cur->m_yielderNextState;
        cur->m_yielder.reset();
    }
    MORDOR_ASSERT(cur->m_dg);
    Fiber *curp = cur.get();
    try {
        if (cur->m_state == EXCEPT) {
            MORDOR_ASSERT(cur->m_exception);
            Mordor::rethrow_exception(cur->m_exception);
        }
        MORDOR_ASSERT(cur->m_state == EXEC);
        cur->m_dg();
    } catch (boost::exception &ex) {
        Fiber::weak_ptr dummy = cur;
        removeTopFrames(ex);
        cur->m_exception = boost::current_exception();
        exitPoint(cur, curp, EXCEPT);
        if (curp->m_state == EXCEPT)
            throw;
        cur = dummy.lock();
    } catch (...) {
        Fiber::weak_ptr dummy = cur;
        cur->m_exception = boost::current_exception();
        exitPoint(cur, curp, EXCEPT);
        if (curp->m_state == EXCEPT)
            throw;
        cur = dummy.lock();
    }

    exitPoint(cur, curp, TERM);
#ifndef NATIVE_WINDOWS_FIBERS
    MORDOR_NOTREACHED();
#endif
}

void
Fiber::exitPoint(Fiber::ptr &cur, Fiber *curp, State targetState)
{
    if (!curp->m_terminateOuter.expired() && !curp->m_outer) {
        ptr terminateOuter(curp->m_terminateOuter);
        // Have to set this reference before calling yieldTo()
        // so we can reset cur before we call yieldTo()
        // (since it's not ever going to destruct)
        terminateOuter->m_yielder = cur;
        terminateOuter->m_yielderNextState = targetState;
        Fiber* terminateOuterp = terminateOuter.get();
        if (cur) {
            MORDOR_ASSERT(!cur.unique());
            cur.reset();
        }
        MORDOR_ASSERT(!terminateOuter.unique());
        terminateOuter.reset();
        terminateOuterp->yieldTo(false, targetState);
        return;
    }
    MORDOR_ASSERT(curp->m_outer);
    curp->m_outer->m_yielder = cur;
    curp->m_outer->m_yielderNextState = targetState;
    if (cur) {
        MORDOR_ASSERT(!cur.unique());
        cur.reset();
    }
    fiber_switchContext(&curp->m_sp, curp->m_outer->m_sp);
}

#if !defined(NATIVE_WINDOWS_FIBERS) && !defined(UCONTEXT_FIBERS) && !defined(SETJMP_FIBERS)
static
void
push(void *&sp, size_t v)
{
    size_t *ssp = (size_t *)sp;
    *--ssp = v;
    sp = (void *)ssp;
}
#endif

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
#ifdef LINUX
    m_valgrindStackId = VALGRIND_STACK_REGISTER(m_stack, (char *)m_stack + m_stacksize);
#endif
    m_sp = (char*)m_stack + m_stacksize;
#endif
}

void
Fiber::freeStack()
{
#ifdef NATIVE_WINDOWS_FIBERS
    DeleteFiber(m_stack);
#elif defined(WINDOWS)
    VirtualFree(m_stack, 0, MEM_RELEASE);
#elif defined(POSIX)
#ifdef LINUX
    VALGRIND_STACK_DEREGISTER(m_valgrindStackId);
#endif
    munmap(m_stack, m_stacksize);
#endif
}

#ifdef __GNUC__

#ifdef ASM_X86_POSIX_FIBERS
extern "C" { int getEbx(); }
#endif
#endif

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
#elif defined(_MSC_VER) && defined(ASM_X86_WINDOWS_FIBERS)
static
void
__declspec(naked)  __cdecl
fiber_switchContext(void **oldsp, void *newsp)
{
    __asm {
        // save current stack state
        push ebp;
        mov ebp, esp;
        // save exception handler and stack
        // info in the TIB
        push dword ptr FS:[0];
        push dword ptr FS:[4];
        push dword ptr FS:[8];
        push ebx;
        push esi;
        push edi;

        // store oldsp
        mov eax, dword ptr 8[ebp];
        mov [eax], esp;
        // load newsp to begin context switch
        mov esp, dword ptr 12[ebp];

        // load saved state from new stack
        pop edi;
        pop esi;
        pop ebx;
        pop dword ptr FS:[8];
        pop dword ptr FS:[4];
        pop dword ptr FS:[0];
        pop ebp;

        ret;
    }
}
#endif


void
Fiber::initStack()
{
#ifdef NATIVE_WINDOWS_FIBERS
    if (m_stack)
        return;
    m_sp = m_stack = CreateFiber(m_stacksize, &native_fiber_entryPoint, &Fiber::entryPoint);
    if (!m_stack)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateFiber");
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
#else
    long long *env = (long long *)m_env;
    env[1] = 0xffffffffffffffffll; // RBP
    env[2] = (long long)m_stack + m_stacksize; // RSP
#endif
#else
#error Platform not supported
#endif
#elif defined(ASM_X86_64_WINDOWS_FIBERS)
    // Shadow space (4 registers + return address)
    for (int i = 0; i < 5; ++i)
        push(m_sp, 0x0000000000000000ull);
    push(m_sp, (size_t)&Fiber::entryPoint);     // RIP
    push(m_sp, 0xffffffffffffffffull);  // RBP
    push(m_sp, (size_t)m_stack + m_stacksize); // Stack base
    push(m_sp, (size_t)m_stack);        // Stack top
    push(m_sp, 0x0000000000000000ull);  // RBX
    push(m_sp, 0x0000000000000000ull);  // RSI
    push(m_sp, 0x0000000000000000ull);  // RDI
    push(m_sp, 0x0000000000000000ull);  // R12
    push(m_sp, 0x0000000000000000ull);  // R13
    push(m_sp, 0x0000000000000000ull);  // R14
    push(m_sp, 0x0000000000000000ull);  // R15
    push(m_sp, 0x00001f8001df0000ull);  // MXCSR (32 bits), x87 control (16 bits), (unused)
    // XMM6:15
    for (int i = 6; i <= 15; ++i) {
        push(m_sp, 0x0000000000000000ull);
        push(m_sp, 0x0000000000000000ull);
    };
#elif defined(ASM_X86_WINDOWS_FIBERS)
    push(m_sp, (size_t)&Fiber::entryPoint); // EIP
    push(m_sp, 0xffffffff);             // EBP
    push(m_sp, 0xffffffff);             // Exception handler
    push(m_sp, (size_t)m_stack + m_stacksize); // Stack base
    push(m_sp, (size_t)m_stack);        // Stack top
    push(m_sp, 0x00000000);             // EBX
    push(m_sp, 0x00000000);             // ESI
    push(m_sp, 0x00000000);             // EDI
#elif defined(ASM_X86_64_POSIX_FIBERS)
    push(m_sp, 0x0000000000000000ull);  // empty space to align to 16 bytes
    push(m_sp, (size_t)&Fiber::entryPoint); // RIP
    push(m_sp, (size_t)m_sp + 8);       // RBP
    push(m_sp, 0x0000000000000000ull);  // RBX
    push(m_sp, 0x0000000000000000ull);  // R12
    push(m_sp, 0x0000000000000000ull);  // R13
    push(m_sp, 0x0000000000000000ull);  // R14
    push(m_sp, 0x0000000000000000ull);  // R15
    push(m_sp, 0x00001f8001df0000ull);  // MXCSR (32 bits), x87 control (16 bits), (unused)
    push(m_sp, 0x0000000000000000ull);  // empty space to align to 16 bytes
#elif defined(ASM_X86_POSIX_FIBERS)
    push(m_sp, 0x00000000);             // empty space to align to 16 bytes
    push(m_sp, (size_t)&Fiber::entryPoint); // EIP
    push(m_sp, (size_t)m_sp + 8);       // EBP
    push(m_sp, 0x00000000);             // EAX
    push(m_sp, getEbx());               // EBX used for PIC code
    push(m_sp, 0x00000000);             // ECX (for alignment)
    push(m_sp, 0x00000000);             // ESI
    push(m_sp, 0x00000000);             // EDI
#endif
}

}
