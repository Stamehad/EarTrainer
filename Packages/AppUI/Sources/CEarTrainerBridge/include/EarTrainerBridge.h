#pragma once

#ifdef __cplusplus
extern "C" {
#endif

char *set_storage_root(const char *path);
char *load_profile(const char *name);
char *serialize_profile(void);
char *deserialize_profile(const char *json);
char *serialize_checkpoint(void);
char *deserialize_checkpoint(const char *json);
int has_active_session(void);
char *start_session(const char *spec_json);
char *next_question(void);
char *feedback(const char *answer_json);
char *end_session(void);
void free_string(char *ptr);

#ifdef __cplusplus
}
#endif
