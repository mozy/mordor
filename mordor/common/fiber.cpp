// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

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

static void allocStack(void **stack, void **sp, size_t *stacksize);
static void freeStack(void *stack, size_t stacksize);
static void initStack(void **stack, void **sp, size_t stacksize, void (*entryPoint)());
#if defined(NATIVE_WINDOWS_FIBERS) || defined(UCONTEXT_FIBERS)
static
#else
static void push(void **sp, size_t v);
extern "C"
#endif
void fiber_switchContext(void **oldsp, void *newsp);

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
    allocStack(&m_stack, &m_sp, &m_stacksize);
#ifdef UCONTEXT_FIBERS
    m_sp = &m_ctx;
#endif
    initStack(&m_stack, &m_sp, m_stacksize, &Fiber::entryPoint);
}

Fiber::~Fiber()
{
    if (m_state == EXCEPT) {
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
        freeStack(m_stack, m_stacksize);
    }
}

void
Fiber::reset()
{
    if (m_state == EXCEPT)
        call(true);
    MORDOR_ASSERT(m_stack);
    MORDOR_ASSERT(m_state == TERM || m_state == INIT);
    MORDOR_ASSERT(m_dg);
    initStack(&m_stack, &m_sp, m_stacksize, &Fiber::entryPoint);
    m_state = INIT;
    m_exception = boost::exception_ptr();
}

void
Fiber::reset(boost::function<void ()> dg)
{
    if (m_state == EXCEPT)
        call(true);
    MORDOR_ASSERT(m_stack);
    MORDOR_ASSERT(m_state == TERM || m_state == INIT);
    m_dg = dg;
    initStack(&m_stack, &m_sp, m_stacksize, &Fiber::entryPoint);
    m_state = INIT;
    m_exception = boost::exception_ptr();
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
    m_state = EXEC;
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
        previous->m_terminateOuter = cur->shared_from_this();
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
    MORDOR_ASSERT(curp->m_yielder || targetState == EXCEPT);
    if (curp->m_yielder) {
        Fiber::ptr yielder = curp->m_yielder;
        yielder->m_state = curp->m_yielderNextState;
        curp->m_yielder.reset();
        if (yielder->m_exception)
            Mordor::rethrow_exception(yielder->m_exception);
        return yielder;
    }
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
    MORDOR_ASSERT(cur->m_state == EXEC);
    Fiber *curp = cur.get();
    try {
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

#if !defined(NATIVE_WINDOWS_FIBERS) && !defined(UCONTEXT_FIBERS)
static
void
push(void **sp, size_t v)
{
    size_t* ssp = (size_t*)*sp;
    *--ssp = v;
    *(size_t**)sp = ssp;
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

static
void
allocStack(void **stack, void **sp, size_t *stacksize)
{
    MORDOR_ASSERT(stack);
    MORDOR_ASSERT(sp);
    if (*stacksize == 0)
        *stacksize = g_defaultStackSize->val();
#ifdef NATIVE_WINDOWS_FIBERS
    // Fibers are allocated in initStack
#elif defined(WINDOWS)
    *stack = VirtualAlloc(NULL, *stacksize + g_pagesize, MEM_RESERVE, PAGE_NOACCESS);
    if (!*stack)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("VirtualAlloc");
    VirtualAlloc(*stack, g_pagesize, MEM_COMMIT, PAGE_READWRITE | PAGE_GUARD);
    // TODO: don't commit until referenced
    VirtualAlloc((char*)*stack + g_pagesize, *stacksize, MEM_COMMIT, PAGE_READWRITE);
    *sp = (char*)*stack + *stacksize + g_pagesize;
#elif defined(POSIX)
    *stack = mmap(NULL, *stacksize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (*stack == MAP_FAILED)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("mmap");
    *sp = (char*)*stack + *stacksize;
#endif
}

static
void
freeStack(void *stack, size_t stacksize)
{
#ifdef NATIVE_WINDOWS_FIBERS
    DeleteFiber(stack);
#elif defined(WINDOWS)
    VirtualFree(stack, 0, MEM_RELEASE);
#elif defined(POSIX)
    munmap(stack, stacksize);
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

static
void
initStack(void **stack, void **sp, size_t stacksize, void (*entryPoint)())
{
#ifdef NATIVE_WINDOWS_FIBERS
    if (*stack)
        return;
    *sp = *stack = CreateFiber(stacksize, &native_fiber_entryPoint, entryPoint);
    if (!*stack)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateFiber");
#elif defined(UCONTEXT_FIBERS)
    ucontext_t *ctx = *(ucontext_t**)sp;
    if (getcontext(ctx))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("getcontext");
    ctx->uc_stack.ss_sp = *stack;
    ctx->uc_stack.ss_size = stacksize;
    makecontext(ctx, entryPoint, 0);
#elif defined(ASM_X86_64_WINDOWS_FIBERS)
    // Shadow space (4 registers + return address)
    for (int i = 0; i < 5; ++i)
        push(sp, 0x0000000000000000ull);
    push(sp, (size_t)entryPoint);     // RIP
    push(sp, 0xffffffffffffffffull);  // RBP
    push(sp, (size_t)*stack + stacksize);// Stack base
    push(sp, (size_t)*stack);          // Stack top
    push(sp, 0x0000000000000000ull);  // RBX
    push(sp, 0x0000000000000000ull);  // RSI
    push(sp, 0x0000000000000000ull);  // RDI
    push(sp, 0x0000000000000000ull);  // R12
    push(sp, 0x0000000000000000ull);  // R13
    push(sp, 0x0000000000000000ull);  // R14
    push(sp, 0x0000000000000000ull);  // R15
    push(sp, 0x00001f8001df0000ull);  // MXCSR (32 bits), x87 control (16 bits), (unused)
    // XMM6:15
    for (int i = 6; i <= 15; ++i) {
        push(sp, 0x0000000000000000ull);
        push(sp, 0x0000000000000000ull);
    };
#elif defined(ASM_X86_WINDOWS_FIBERS)
    push(sp, (size_t)entryPoint);     // EIP
    push(sp, 0xffffffff);             // EBP
    push(sp, 0xffffffff);             // Exception handler
    push(sp, (size_t)*stack + stacksize);// Stack base
    push(sp, (size_t)*stack);          // Stack top
    push(sp, 0x00000000);             // EBX
    push(sp, 0x00000000);             // ESI
    push(sp, 0x00000000);             // EDI
#elif defined(ASM_X86_64_POSIX_FIBERS)
    push(sp, 0x0000000000000000ull);             // empty space to align to 16 bytes
    push(sp, (size_t)entryPoint);     // RIP
    push(sp, (size_t)*sp + 8);        // RBP
    push(sp, 0x0000000000000000ull);  // RBX
    push(sp, 0x0000000000000000ull);  // R12
    push(sp, 0x0000000000000000ull);  // R13
    push(sp, 0x0000000000000000ull);  // R14
    push(sp, 0x0000000000000000ull);  // R15
    push(sp, 0x00001f8001df0000ull);  // MXCSR (32 bits), x87 control (16 bits), (unused)
    push(sp, 0x0000000000000000ull);  // empty space to align to 16 bytes
#elif defined(ASM_X86_POSIX_FIBERS)
    push(sp, 0x00000000);             // empty space to align to 16 bytes
    push(sp, (size_t)entryPoint);     // EIP
    push(sp, (size_t)*sp + 8);        // EBP
    push(sp, 0x00000000);             // EAX
    push(sp, getEbx());               // EBX used for PIC code
    push(sp, 0x00000000);             // ECX (for alignment)
    push(sp, 0x00000000);             // ESI
    push(sp, 0x00000000);             // EDI
#endif
}

}
