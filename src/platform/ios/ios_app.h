#pragma once

#ifdef __cplusplus
extern "C" {
#endif
void* tl_ios_view_handle(void);
void* tl_ios_controller_handle(void);
bool tl_ios_app_active(void);
const char* tl_ios_program_folder(void);
#ifdef __cplusplus
}
#endif
