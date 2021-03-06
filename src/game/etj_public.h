#pragma once

#if defined(__linux__)
#define FN_PUBLIC __attribute__((visibility("default")))
#elif defined(_WIN32)
// https://msdn.microsoft.com/en-us/library/wf2w9f6x.aspx
#define FN_PUBLIC __declspec(dllexport)
#else
#error "Unsupported compiler"
#endif
