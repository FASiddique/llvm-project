include "config/public_api.td"

include "spec/linux.td"
include "spec/posix.td"
include "spec/stdc.td"

def SizeT : TypeDecl<"size_t"> {
  let Decl = [{
    #define __need_size_t
    #include <stddef.h>
  }];
}

def OffT : TypeDecl<"off_t"> {
  let Decl = [{
    #define __need_off_t
    #include <__posix-types.h>
  }];
}

def FILE : TypeDecl<"FILE"> {
  let Decl = [{
    typedef struct FILE FILE;
  }];
}

def AssertMacro : MacroDef<"assert"> {
  let Defn = [{
    #undef assert

    #ifdef NDEBUG
    #define assert(e) (void)0
    #else

    #ifdef __cplusplus
    extern "C"
    #endif
    _Noreturn void __assert_fail(const char *, const char *, unsigned, const char *);

    #define assert(e)  \
      ((e) ? (void)0 : __assert_fail(#e, __FILE__, __LINE__, __PRETTY_FUNCTION__))

    #endif
  }];
}

def StaticAssertMacro : MacroDef<"static_assert"> {
  let Defn = [{
    #ifndef __cplusplus
    #undef static_assert
    #define static_assert _Static_assert
    #endif
  }];
}

def NullMacro : MacroDef<"NULL"> {
  let Defn = [{
    #define __need_NULL
    #include <stddef.h>
  }];
}

def ErrnoMacro : MacroDef<"errno"> {
  let Defn = [{
    #ifdef __cplusplus
    extern "C"
    #endif
    int *__errno_location();
    #define errno (*__errno_location())
  }];
}

def AssertAPI : PublicAPI<"assert.h"> {
  let Macros = [
    AssertMacro,
    StaticAssertMacro,
  ];
}

def MathErrHandlingMacro : MacroDef<"math_errhandling"> {
  let Defn = [{
    #ifndef math_errhandling
    #ifdef __FAST_MATH__
    #define math_errhandling 0
    #elif defined __NO_MATH_ERRNO__
    #define math_errhandling (MATH_ERREXCEPT)
    #else
    #define math_errhandling (MATH_ERRNO | MATH_ERREXCEPT)
    #endif
    #endif // math_errhandling not defined
  }];
}

def IsFiniteMacro : MacroDef<"isfinite"> {
  let Defn = [{
    #define isfinite(x) __builtin_isfinite(x)
  }];
}

def IsInfMacro : MacroDef<"isinf"> {
  let Defn = [{
    #define isinf(x) __builtin_isinf(x)
  }];
}

def IsNanMacro : MacroDef<"isnan"> {
  let Defn = [{
    #define isnan(x) __builtin_isnan(x)
  }];
}

def MathAPI : PublicAPI<"math.h"> {
  let Macros = [
    SimpleMacroDef<"MATH_ERRNO", "1">,
    SimpleMacroDef<"MATH_ERREXCEPT", "2">,
    MathErrHandlingMacro,

    SimpleMacroDef<"INFINITY", "__builtin_inff()">,
    SimpleMacroDef<"NAN", "__builtin_nanf(\"\")">,

    IsFiniteMacro,
    IsInfMacro,
    IsNanMacro,
  ];
  let Functions = [
   "round",
  ];
}

def StringAPI : PublicAPI<"string.h"> {
  let Functions = [
    "memcpy",
    "memmove",
    "memcmp",
    "memchr",
    "memset",
    "strcpy",
    "strncpy",
    "strcat",
    "strncat",
    "strcmp",
    "strcoll",
    "strncmp",
    "strxfrm",
    "strchr",
    "strcspn",
    "strpbrk",
    "strrchr",
    "strspn",
    "strstr",
    "strtok",
    "strerror",
    "strlen",
  ];

  let TypeDeclarations = [
    SizeT,
  ];

  let Macros = [
    NullMacro,
  ];
}

def StdIOAPI : PublicAPI<"stdio.h"> {
  let TypeDeclarations = [
    SizeT,
    FILE,
  ];

  let Functions = [
    "fwrite",
  ];
}

def StdlibAPI : PublicAPI<"stdlib.h"> {
  let Functions = [
    "_Exit",
    "abort",
  ];
}

def ErrnoAPI : PublicAPI<"errno.h"> {
  let Macros = [
    ErrnoMacro,
    // We largely depend on linux/errno.h to give us the
    // various error macro definitions. However, some libc
    // implementations have chosen to provide definitions
    // for some of the error macros to account for the ones
    // missing in linux/errno.h. There is no harm in doing
    // the same here if we define the macros only when they
    // are not already defined.
    MacroDefineIfNot<"ENOTSUP", "EOPNOTSUPP">,
    MacroDefineIfNot<"ECANCELED", "125">,
    MacroDefineIfNot<"EOWNERDEAD", "130">,
    MacroDefineIfNot<"ENOTRECOVERABLE", "131">,
    MacroDefineIfNot<"ERFKILL", "132">,
    MacroDefineIfNot<"EHWPOISON", "133">,
  ];
}

def SysMManAPI : PublicAPI<"sys/mman.h"> {
  let Macros = [
    SimpleMacroDef<"PROT_NONE", "0">,
    SimpleMacroDef<"PROT_READ", "1">,
    SimpleMacroDef<"PROT_WRITE", "2">,
    SimpleMacroDef<"PROT_EXEC", "4">,

    SimpleMacroDef<"MAP_FIXED", "1">,
    SimpleMacroDef<"MAP_PRIVATE", "2">,
    SimpleMacroDef<"MAP_SHARED", "4">,

    SimpleMacroDef<"MAP_FAILED", "((void*)-1)">,

    // TODO: The value of 0x20 is good for x86_64, but has to be extended
    // in some manner to accommodate other machine architectures.
    SimpleMacroDef<"MAP_ANONYMOUS", "0x20">

    // TODO: Add other MAP_* macros used by Linux.
  ];

  let TypeDeclarations = [
    SizeT,
    OffT,
  ];

  let Functions = [
    "mmap",
    "munmap",
  ];
}

def StructSigactionDefn : TypeDecl<"struct sigaction"> {
  let Decl = [{
    struct __sigaction {
      union {
        void (*sa_handler)(int);
        void (*sa_action)(int, siginfo_t *, void *);
      };
      sigset_t sa_mask;
      int sa_flags;
      void (*sa_restorer)(void);
    };
  }];
}

def SighandlerTDefn : TypeDecl<"__sighandler_t"> {
  let Decl = [{
    typedef void(*__sighandler_t)(int);
  }];
}

def SignalAPI : PublicAPI<"signal.h"> {
  let TypeDeclarations = [
    StructSigactionDefn,
    SighandlerTDefn,
  ];

  let Functions = [
    "raise",
    "sigaction",
    "sigprocmask",
    "sigemptyset",
    "sigaddset",
    "signal",
  ];
}

def MtxT : TypeDecl<"mtx_t"> {
  let Decl = [{
    typedef struct {
      unsigned char __internal_data[4];
      int __mtx_type;
    } mtx_t;
  }];
}

def ThreadStartT : TypeDecl<"thrd_start_t"> {
  let Decl = "typedef int (*thrd_start_t)(void *);";
}

def ThreadsAPI : PublicAPI<"threads.h"> {
  let TypeDeclarations = [
    MtxT,
    ThreadStartT,
  ];

  let Enumerations = [
    "mtx_plain",
    "mtx_recursive",
    "mtx_timed",
    "thrd_timedout",
    "thrd_success",
    "thrd_busy",
    "thrd_error",
    "thrd_nomem",
  ];

  let Functions = [
    "mtx_init",
    "mtx_lock",
    "mtx_unlock",
    "thrd_create",
    "thrd_join",
  ];
}
