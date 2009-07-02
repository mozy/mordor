// Copyright (c) 2009 - Decho Corp.

#include "fiber.h"
#include "version.h"

#include <cassert>

#ifdef WINDOWS

#define NATIVE_WINDOWS_FIBERS

#include <windows.h>
#else
#include <sys/mman.h>
#include <pthread.h>
#endif

#ifdef X86_64
#   ifdef WINDOWS
#       define ASM_X86_64_WINDOWS
#   elif defined(POSIX)
#       define ASM_X86_64_POSIX
#   endif
#elif defined(X86)
#   ifdef WINDOWS
#       define ASM_X86_WINDOWS
#   elif defined(POSIX)
#       define ASM_X86_POSIX
#   endif
#else
#   error Platform not supported
#endif

static void push(void **sp, size_t v);
static void allocStack(void **stack, void **sp, size_t *stacksize, void (*entryPoint)());
static void freeStack(void *stack, size_t stacksize);
static void initStack(void *stack, void **sp, size_t stacksize, void (*entryPoint)());
extern "C"
#ifdef NATIVE_WINDOWS_FIBERS
static
#endif
void fiber_switchContext(void **oldsp, void *newsp);

static size_t g_pagesize;

struct FiberInitializer {
    FiberInitializer()
    {
#ifdef WINDOWS
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        g_pagesize = info.dwPageSize;
#elif defined(POSIX)
        g_pagesize = 4096;
#endif
    }
};

static FiberInitializer g_init;

static void delete_nothing(Fiber* f) {}

boost::thread_specific_ptr<Fiber> Fiber::t_fiber(&delete_nothing);

Fiber::Fiber()
{
    assert(!getThis());
    m_state = EXEC;
    m_stack = NULL;
    m_stacksize = 0;
    m_sp = NULL;
    setThis(this);
#ifdef NATIVE_WINDOWS_FIBERS
    m_sp = ConvertThreadToFiber(NULL);
#endif
}

Fiber::Fiber(boost::function<void ()> dg, size_t stacksize)
{
    stacksize += g_pagesize - 1;
    stacksize -= stacksize % g_pagesize;
    m_dg = dg;
    m_state = HOLD;
    m_stacksize = stacksize;
    allocStack(&m_stack, &m_sp, &m_stacksize, &Fiber::entryPoint);
    initStack(m_stack, &m_sp, m_stacksize, &Fiber::entryPoint);
}

Fiber::~Fiber()
{
    if (!m_stack) {
        assert(!m_dg);
        assert(m_state == EXEC);
        Fiber *cur = t_fiber.get();
        assert(cur);
        assert(cur == this);
        setThis(NULL);
#ifdef NATIVE_WINDOWS_FIBERS
        ConvertFiberToThread();
#endif
    } else {
        assert(m_state == TERM);
        freeStack(m_stack, m_stacksize);
    }
}

void
Fiber::reset()
{
    assert(m_dg);
    assert(m_state == TERM);
    initStack(m_stack, &m_sp, m_stacksize, &Fiber::entryPoint);
    m_state = HOLD;
}

void
Fiber::reset(boost::function<void ()> dg)
{
    assert(m_stack);
    assert(m_state == TERM || (!m_dg && m_state == HOLD));
    m_dg = dg;
    initStack(m_stack, &m_sp, m_stacksize, &Fiber::entryPoint);
    m_state = HOLD;
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
    assert(m_state == HOLD);
    assert(!m_outer);
    ptr cur = getThis();
    assert(cur);
    assert(cur.get() != this);
    setThis(this);
    m_outer = cur;
    m_state = EXEC;
    fiber_switchContext(&cur->m_sp, m_sp);
    setThis(cur.get());
    assert(cur->m_yielder);
    assert(cur->m_yielder.get() == this);
    cur->m_yielder->m_state = cur->m_yielderNextState;
    cur->m_yielder.reset();
    m_outer.reset();
}

void
Fiber::yieldTo(bool yieldToCallerOnTerminate)
{
    yieldTo(yieldToCallerOnTerminate, false);
}

void
Fiber::yield()
{
    ptr cur = getThis();
    assert(cur);
    assert(cur->m_state == EXEC);
    assert(cur->m_outer);
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
Fiber::yieldTo(bool yieldToCallerOnTerminate, bool terminateMe)
{
    assert(m_state == HOLD);
    ptr cur = getThis();
    assert(cur);
    setThis(this);
    if (yieldToCallerOnTerminate)
        m_terminateOuter = cur->shared_from_this();
    m_state = EXEC;
    m_yielder = cur;
    m_yielderNextState = terminateMe ? TERM : HOLD;
    Fiber *curp = cur.get();
    // Relinguish our reference
    if (terminateMe) {
        cur.reset();
    }
    fiber_switchContext(&curp->m_sp, m_sp);
    assert(!terminateMe);
    setThis(cur.get());
    if (cur->m_yielder) {
        cur->m_yielder->m_state = cur->m_yielderNextState;
        cur->m_yielder.reset();
    }
}

void
Fiber::entryPoint()
{
    ptr cur = getThis();
    assert(cur);
    if (cur->m_yielder) {
        cur->m_yielder->m_state = cur->m_yielderNextState;
        cur->m_yielder.reset();
    }
    assert(cur->m_dg);
    cur->m_dg();

    if (!cur->m_terminateOuter.expired() && !cur->m_outer) {
        ptr terminateOuter(cur->m_terminateOuter);
        // Have to set this reference before calling yieldTo()
        // so we can reset cur before we call yieldTo()
        // (since it's not ever going to destruct)
        terminateOuter->m_yielder = cur;
        Fiber* terminateOuterp = terminateOuter.get();
        assert(!cur.unique());
        assert(!terminateOuter.unique());
        cur.reset();
        terminateOuter.reset();
        terminateOuterp->yieldTo(false, true);
    }
    assert(cur->m_outer);
    cur->m_outer->m_yielder = cur;
    cur->m_outer->m_yielderNextState = Fiber::TERM;
    Fiber* curp = cur.get();
    assert(!cur.unique());
    cur.reset();
    fiber_switchContext(&curp->m_sp, curp->m_outer->m_sp);
}

static
void
push(void **sp, size_t v)
{
    size_t* ssp = (size_t*)*sp;
    *--ssp = v;
    *(size_t**)sp = ssp;
}

#ifdef NATIVE_WINDOWS_FIBERS
static VOID CALLBACK native_fiber_entryPoint(PVOID lpParameter)
{
    void (*entryPoint)() = (void (*)())lpParameter;
    entryPoint();
}
#endif

static
void
allocStack(void **stack, void **sp, size_t *stacksize, void (*entryPoint)())
{
    assert(stack);
    assert(sp);
    if (*stacksize == 0)
        *stacksize = g_pagesize;
#ifdef NATIVE_WINDOWS_FIBERS
    *sp = *stack = CreateFiber(*stacksize, &native_fiber_entryPoint, entryPoint);
    if (!*stack) {
        assert(false);
    }
#elif defined(WINDOWS)
    *stack = VirtualAlloc(NULL, *stacksize + g_pagesize, MEM_RESERVE, PAGE_NOACCESS);
    if (!*stack) {
        assert(false);
    }
    VirtualAlloc(*stack, g_pagesize, MEM_COMMIT, PAGE_READWRITE | PAGE_GUARD);
    VirtualAlloc((char*)*stack + g_pagesize, *stacksize, MEM_COMMIT, PAGE_READWRITE);
    *sp = (char*)*stack + *stacksize + g_pagesize;
#elif defined(POSIX)
    *stack = mmap(NULL, *stacksize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (*stack == MAP_FAILED) {
        assert(false);
    }
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

#ifdef ASM_X86_POSIX
extern "C" { int getEbx(); }
#endif
#endif

#ifdef NATIVE_WINDOWS_FIBERS
extern "C" static void
fiber_switchContext(void **oldsp, void *newsp)
{
    SwitchToFiber(newsp);
}
#else
#ifdef _MSC_VER
#ifdef ASM_X86_WINDOWS
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
#endif
#endif

static
void
initStack(void *stack, void **sp, size_t stacksize, void (*entryPoint)())
{
#ifdef NATIVE_WINDOWS_FIBERS
#elif defined(ASM_X86_64_WINDOWS)
    // Shadow space (4 registers + return address)
    for (int i = 0; i < 5; ++i)
        push(sp, 0x0000000000000000ull);
    push(sp, (size_t)entryPoint);     // RIP
    push(sp, 0xffffffffffffffffull);  // RBP
    push(sp, (size_t)stack + stacksize);// Stack base
    push(sp, (size_t)stack);          // Stack top
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
#elif defined(ASM_X86_WINDOWS)
    push(sp, (size_t)entryPoint);     // EIP
    push(sp, 0xffffffff);             // EBP
    push(sp, 0xffffffff);             // Exception handler
    push(sp, (size_t)stack + stacksize);// Stack base
    push(sp, (size_t)stack);          // Stack top
    push(sp, 0x00000000);             // EBX
    push(sp, 0x00000000);             // ESI
    push(sp, 0x00000000);             // EDI
#elif defined(ASM_X86_64_POSIX)
    push(sp, (size_t)entryPoint);     // RIP
    push(sp, (size_t)*sp + 8);        // RBP
    push(sp, 0x0000000000000000ull);  // RBX
    push(sp, 0x0000000000000000ull);  // R12
    push(sp, 0x0000000000000000ull);  // R13
    push(sp, 0x0000000000000000ull);  // R14
    push(sp, 0x0000000000000000ull);  // R15
    push(sp, 0x00001f8001df0000ull);  // MXCSR (32 bits), x87 control (16 bits), (unused)
#elif defined(ASM_X86_POSIX)
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
