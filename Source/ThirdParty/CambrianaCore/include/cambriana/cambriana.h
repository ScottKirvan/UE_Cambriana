#ifndef CAMBRIANA_CAMBRIANA_H
#define CAMBRIANA_CAMBRIANA_H

#include <stdint.h>

#if defined(_WIN32)
#  if defined(CAMBRIANA_BUILDING_LIBRARY)
#    define CAMBRIANA_API __declspec(dllexport)
#  else
#    define CAMBRIANA_API __declspec(dllimport)
#  endif
#else
#  define CAMBRIANA_API __attribute__((visibility("default")))
#endif

#define CAMBRIANA_API_VERSION 1u

#ifdef __cplusplus
extern "C" {
#endif

CAMBRIANA_API uint32_t cambriana_api_version(void);

#ifdef __cplusplus
}
#endif

#endif
