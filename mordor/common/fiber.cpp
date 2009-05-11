// Copyright (c) 2009 - Decho Corp.

#include "fiber.h"
#include "version.h"

#include <cassert>

#ifdef WINDOWS
#define WIN32_LEAN_AND_MEAN
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
static void allocStack(void **stack, void **sp, size_t *stacksize);
static void freeStack(void *stack, size_t stacksize);
static void initStack(void *stack, void **sp, size_t stacksize, void (*entryPoint)());
extern "C" { void fiber_switchContext(void **oldsp, void *newsp); }

#ifdef WINDOWS
static DWORD g_tls;
#elif defined(POSIX)
static pthread_key_t g_tls;
#endif
static size_t g_pagesize;

struct Initializer {
    Initializer()
    {
#ifdef WINDOWS
        g_tls = TlsAlloc();
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        g_pagesize = info.dwPageSize;
#elif defined(POSIX)
        int rc = pthread_key_create(&g_tls, NULL);
        assert(rc == 0);
        g_pagesize = 4096;
#endif
    }

    ~Initializer()
    {
#ifdef WINDOWS
        TlsFree(g_tls);
#elif defined(POSIX)
        pthread_key_delete(g_tls);
#endif
    }
};

static Initializer g_init;

Fiber::Fiber()
{
    assert(!getThis());
    m_fn = NULL;
    m_state = EXEC;
    m_stacksize = 0;
    m_outer = m_terminateOuter = m_yielder = NULL;
    m_sp = NULL;
    setThis(this);
}

Fiber::Fiber(void (*fn)(), size_t stacksize)
{
    assert(fn);
    stacksize += g_pagesize - 1;
    stacksize -= stacksize % g_pagesize;
    m_fn = fn;
    m_state = HOLD;
    m_stacksize = stacksize;
    m_outer = m_terminateOuter = m_yielder = NULL;
    allocStack(&m_stack, &m_sp, &m_stacksize);
    initStack(m_stack, &m_sp, m_stacksize, &Fiber::entryPoint);
}

Fiber::~Fiber()
{
    if (!m_fn) {
        assert(m_state == EXEC);
        assert(getThis() == this);
        setThis(NULL);
    } else {
        assert(m_state == TERM);
        freeStack(m_stack, m_stacksize);
    }
}

void
Fiber::reset()
{
    assert(m_fn);
    assert(m_state == TERM);
    initStack(m_stack, &m_sp, m_stacksize, &Fiber::entryPoint);
    m_state = HOLD;
}

void
Fiber::reset(void (*fn)())
{
    assert(m_fn);
    assert(m_state == TERM);
    m_fn = fn;
    initStack(m_stack, &m_sp, m_stacksize, &Fiber::entryPoint);
    m_state = HOLD;
}

Fiber*
Fiber::getThis()
{
#ifdef WINDOWS
    return (Fiber*)TlsGetValue(g_tls);
#elif defined(POSIX)
    return (Fiber*)pthread_getspecific(g_tls);
#endif
}

void
Fiber::setThis(Fiber* f)
{
#ifdef WINDOWS
    TlsSetValue(g_tls, f);
#elif defined(POSIX)
    pthread_setspecific(g_tls, f);
#endif
}

void
Fiber::call()
{
    assert(m_state == HOLD);
    assert(!m_outer);
    Fiber* cur = getThis();
    assert(cur);
    setThis(this);
    m_outer = cur;
    m_state = EXEC;
    fiber_switchContext(&cur->m_sp, m_sp);
    setThis(cur);
    assert(cur->m_yielder);
    assert(cur->m_yielder == this);
    cur->m_yielder->m_state = cur->m_yielderNextState;
    cur->m_yielder = NULL;
    m_outer = NULL;
}

void
Fiber::yieldTo(bool yieldToCallerOnTerminate)
{
    yieldTo(yieldToCallerOnTerminate, false);
}

void
Fiber::yield()
{
    Fiber* cur = getThis();
    assert(cur);
    assert(cur->m_state == EXEC);
    assert(cur->m_outer);
    cur->m_outer->m_yielder = cur;
    cur->m_outer->m_yielderNextState = Fiber::HOLD;
    fiber_switchContext(&cur->m_sp, cur->m_outer->m_sp);
    if (cur->m_yielder) {
        cur->m_yielder->m_state = cur->m_yielderNextState;
        cur->m_yielder = NULL;
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
    Fiber* cur = getThis();
    assert(cur);
    setThis(this);
    if (yieldToCallerOnTerminate)
        m_terminateOuter = cur;
    m_state = EXEC;
    m_yielder = cur;
    m_yielderNextState = terminateMe ? TERM : HOLD;
    fiber_switchContext(&cur->m_sp, m_sp);
    setThis(cur);
    if (cur->m_yielder) {
        cur->m_yielder->m_state = cur->m_yielderNextState;
        cur->m_yielder = NULL;
    }
}

void
Fiber::entryPoint()
{
    Fiber* cur = getThis();
    assert(cur);
    if (cur->m_yielder) {
        cur->m_yielder->m_state = cur->m_yielderNextState;
        cur->m_yielder = NULL;
    }
    cur->m_fn();

    if (cur->m_terminateOuter && !cur->m_outer) {
        cur->m_terminateOuter->yieldTo(false, true);
    }
    assert(cur->m_outer);
    cur->m_outer->m_yielder = cur;
    cur->m_outer->m_yielderNextState = Fiber::TERM;
    fiber_switchContext(&cur->m_sp, cur->m_outer->m_sp);
}

static
void
push(void **sp, size_t v)
{
    size_t* ssp = (size_t*)*sp;
    *--ssp = v;
    *(size_t**)sp = ssp;
}

static
void
allocStack(void **stack, void **sp, size_t *stacksize)
{
    assert(stack);
    assert(sp);
    if (*stacksize == 0)
        *stacksize = g_pagesize;
#ifdef WINDOWS
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
#ifdef WINDOWS
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
        pop eax;
        pop ebp;

        ret;
    }
}
#endif
#endif

static
void
initStack(void *stack, void **sp, size_t stacksize, void (*entryPoint)())
{
#ifdef ASM_X86_64_WINDOWS
    push(sp, (size_t)entryPoint);     // RIP
    push(sp, 0xffffffffffffffffull);  // RBP
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
    push(sp, 0x00000000);             // EAX
    push(sp, 0xffffffff);             // FS:[0]
    push(sp, (size_t)stack + stacksize);// FS:[4]
    push(sp, (size_t)stack);          // FS:[8]
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
