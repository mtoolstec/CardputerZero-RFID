#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int hal_screenshot_save(const char *dir, char *saved_path, size_t saved_path_size);

#ifdef __cplusplus
}
#endif
