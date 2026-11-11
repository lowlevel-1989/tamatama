/* host/sandbox.c: en Android, los procesos de una app pasan por un filtro
   seccomp que mata (SIGSYS) a quien use una llamada al sistema que bionic no
   usa. loader-arm y qemu-arm son estáticos con glibc, y glibc hace alguna al
   arrancar (set_robust_list, rseq) o al crear threads (clone3).

   Seccomp no mata directamente: manda SIGSYS. Si el proceso está trazado
   (ptrace), la señal para primero en quien lo traza. Aquí un thread
   supervisor crea el hijo, lo traza y, a cada SIGSYS de seccomp, deja la
   llamada como no disponible (-ENOSYS) en lugar de entregar la señal. glibc
   ya sabe vivir sin ellas: sin robust list, sin rseq, clone en lugar de
   clone3. Es lo que hacen PRoot y Termux. access (x86_64) no se rechaza: se
   repite como faccessat, que sí está permitida.

   Solo para en las señales, no en cada llamada al sistema, así que el coste
   es mínimo. Sigue a los threads nuevos del hijo (PTRACE_O_TRACECLONE) y el
   hijo muere con este proceso (PTRACE_O_EXITKILL).

   El thread que hace fork es el que traza, así que el fork también se hace
   en el supervisor. Solo un hijo a la vez. */
#ifdef __ANDROID__
#include "common.h"
#include "sandbox.h"
#include <elf.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef SYS_SECCOMP
#define SYS_SECCOMP 1
#endif

static struct {
    void (*child)(void *);
    void *context;
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t ready;
    pid_t pid;      /* 0 hasta que el hijo existe */
    int status;     /* el de waitpid, al terminar */
} spawn = {.lock = PTHREAD_MUTEX_INITIALIZER, .ready = PTHREAD_COND_INITIALIZER};

/* El valor de vuelta de la llamada que acaba de rechazar seccomp: el primer
   register (x0 o rax; r0 si el hijo es de 32 bits). */
static void set_result(pid_t who, long value) {
    unsigned long long regs[64];
    struct iovec io = {regs, sizeof regs};
    if (ptrace(PTRACE_GETREGSET, who, (void *)NT_PRSTATUS, &io)) return;
#if defined(__aarch64__)
    if (io.iov_len == 18 * 4) ((unsigned *)regs)[0] = (unsigned)value; /* ARM de 32 bits */
    else regs[0] = (unsigned long long)value;
#elif defined(__x86_64__)
    regs[10] = (unsigned long long)value; /* user_regs_struct.rax */
#endif
    ptrace(PTRACE_SETREGSET, who, (void *)NT_PRSTATUS, &io);
}

/* access en x86_64: Android solo deja faccessat, que hace lo mismo con
   AT_FDCWD. Se convierte la llamada y se repite: la instrucción syscall
   ocupa 2 bytes, justo antes de donde paró. 1 si la ha convertido. */
static int retry_as_faccessat(pid_t who, int number) {
#if defined(__x86_64__)
    enum { R10 = 7, RAX = 10, RDX = 12, RSI = 13, RDI = 14, RIP = 16 };
    unsigned long long regs[32];
    struct iovec io = {regs, sizeof regs};
    if (number != 21 || ptrace(PTRACE_GETREGSET, who, (void *)NT_PRSTATUS, &io)) return 0;
    regs[RDX] = regs[RSI];
    regs[RSI] = regs[RDI];
    regs[RDI] = (unsigned long long)-100; /* AT_FDCWD */
    regs[R10] = 0;
    regs[RAX] = 269; /* faccessat */
    regs[RIP] -= 2;
    return !ptrace(PTRACE_SETREGSET, who, (void *)NT_PRSTATUS, &io);
#else
    (void)who, (void)number;
    return 0;
#endif
}

/* Las llamadas rechazadas, una vez cada una, para el log. */
static void report(int number) {
    static unsigned char seen[1024];
    if (number < 0 || number >= (int)sizeof seen || seen[number]) return;
    seen[number] = 1;
    log_msg("[sandbox] system call %d not allowed on Android: answered ENOSYS\n", number);
}

static void *supervise(void *unused) {
    (void)unused;
    pid_t child = fork();
    if (child == 0) {
        ptrace(PTRACE_TRACEME, 0, 0, 0);
        spawn.child(spawn.context);
        _exit(127);
    }
    pthread_mutex_lock(&spawn.lock);
    spawn.pid = child > 0 ? child : -1;
    pthread_cond_signal(&spawn.ready);
    pthread_mutex_unlock(&spawn.lock);
    if (child < 0) return NULL;

    int started = 0;
    for (;;) {
        int status;
        pid_t who = waitpid(-1, &status, __WALL);
        if (who < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            if (who == child) {
                spawn.status = status;
                break;
            }
            continue;
        }
        if (!WIFSTOPPED(status)) continue;
        int signal = WSTOPSIG(status);
        /* la primera parada: el exec */
        if (!started && who == child && signal == SIGTRAP) {
            started = 1;
            ptrace(PTRACE_SETOPTIONS, who, 0, (void *)(long)(PTRACE_O_TRACECLONE | PTRACE_O_EXITKILL));
            ptrace(PTRACE_CONT, who, 0, 0);
            continue;
        }
        /* un thread nuevo: el aviso del clone y su SIGSTOP inicial */
        if (status >> 16 || signal == SIGSTOP) {
            ptrace(PTRACE_CONT, who, 0, 0);
            continue;
        }
        if (signal == SIGSYS) {
            siginfo_t info;
            if (!ptrace(PTRACE_GETSIGINFO, who, 0, &info) && info.si_code == SYS_SECCOMP) {
                if (!retry_as_faccessat(who, info.si_syscall)) {
                    report(info.si_syscall);
                    set_result(who, -ENOSYS);
                }
                ptrace(PTRACE_CONT, who, 0, 0);
                continue;
            }
        }
        /* cualquier otra señal, para el hijo */
        ptrace(PTRACE_CONT, who, 0, (void *)(long)signal);
    }
    return NULL;
}

pid_t sandbox_spawn(void (*child)(void *context), void *context) {
    spawn.child = child;
    spawn.context = context;
    spawn.pid = 0;
    spawn.status = 0;
    if (pthread_create(&spawn.thread, NULL, supervise, NULL)) return -1;
    pthread_mutex_lock(&spawn.lock);
    while (!spawn.pid) pthread_cond_wait(&spawn.ready, &spawn.lock);
    pthread_mutex_unlock(&spawn.lock);
    return spawn.pid;
}

int sandbox_wait(void) {
    pthread_join(spawn.thread, NULL);
    return spawn.status;
}
#endif
