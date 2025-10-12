#include "EarTrainerBridge.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *storage_root;
    char *profile_json;
    bool session_active;
    int current_question;
    int total_questions;
} EngineState;

static EngineState engine_state = {0};

static const char *kQuestionBank[] = {
    "{\"id\":\"q1\",\"prompt\":\"Identify the interval\",\"choices\":[{\"id\":\"A\",\"text\":\"Minor Third\"},{\"id\":\"B\",\"text\":\"Major Third\"},{\"id\":\"C\",\"text\":\"Perfect Fifth\"}],\"metadata\":{\"audio\":\"interval_1\"}}",
    "{\"id\":\"q2\",\"prompt\":\"Identify the chord quality\",\"choices\":[{\"id\":\"A\",\"text\":\"Major\"},{\"id\":\"B\",\"text\":\"Minor\"},{\"id\":\"C\",\"text\":\"Diminished\"}],\"metadata\":{\"audio\":\"chord_1\"}}"
};

static char *duplicate_string(const char *source) {
    if (source == NULL) {
        return NULL;
    }
    size_t length = strlen(source);
    char *copy = (char *)malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, source, length);
    copy[length] = '\0';
    return copy;
}

static char *format_string(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int size = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (size < 0) {
        return NULL;
    }

    char *buffer = (char *)malloc((size_t)size + 1);
    if (buffer == NULL) {
        return NULL;
    }

    va_start(args, format);
    vsnprintf(buffer, (size_t)size + 1, format, args);
    va_end(args);
    return buffer;
}

static void ensure_default_profile(const char *name_hint) {
    if (engine_state.profile_json != NULL) {
        return;
    }
    const char *name_value = (name_hint != NULL && strlen(name_hint) > 0) ? name_hint : "Player";
    engine_state.profile_json = format_string(
        "{\"name\":\"%s\",\"totalSessions\":0,"
        "\"settings\":{\"showLatency\":false,\"enableAssistance\":false,\"useCBOR\":false},"
        "\"updatedAt\":\"1970-01-01T00:00:00Z\"}",
        name_value
    );
}

char *set_storage_root(const char *path) {
    if (engine_state.storage_root != NULL) {
        free(engine_state.storage_root);
    }
    engine_state.storage_root = duplicate_string(path);
    return NULL;
}

char *load_profile(const char *name) {
    ensure_default_profile(name);
    return duplicate_string(engine_state.profile_json);
}

char *serialize_profile(void) {
    ensure_default_profile(NULL);
    return duplicate_string(engine_state.profile_json);
}

char *deserialize_profile(const char *json) {
    if (json == NULL) {
        return duplicate_string("Missing profile json");
    }
    if (engine_state.profile_json != NULL) {
        free(engine_state.profile_json);
    }
    engine_state.profile_json = duplicate_string(json);
    return NULL;
}

char *serialize_checkpoint(void) {
    if (!engine_state.session_active) {
        return NULL;
    }
    return format_string(
        "{\"currentQuestion\":%d,\"totalQuestions\":%d}",
        engine_state.current_question,
        engine_state.total_questions
    );
}

static int extract_int_with_key(const char *json, const char *key, int fallback) {
    if (json == NULL || key == NULL) {
        return fallback;
    }
    const char *location = strstr(json, key);
    if (location == NULL) {
        return fallback;
    }
    location = strchr(location, ':');
    if (location == NULL) {
        return fallback;
    }
    location += 1;
    while (*location == ' ' || *location == '\t') {
        location++;
    }
    return atoi(location);
}

char *deserialize_checkpoint(const char *json) {
    if (json == NULL) {
        return duplicate_string("Missing checkpoint json");
    }
    int index = extract_int_with_key(json, "currentQuestion", 0);
    int total = extract_int_with_key(json, "totalQuestions", 0);
    engine_state.current_question = index;
    engine_state.total_questions = total > 0 ? total : 2;
    engine_state.session_active = index < engine_state.total_questions;
    return NULL;
}

int has_active_session(void) {
    return engine_state.session_active ? 1 : 0;
}

char *start_session(const char *spec_json) {
    (void)spec_json;
    engine_state.session_active = true;
    engine_state.current_question = 0;
    engine_state.total_questions = 2;
    return NULL;
}

char *next_question(void) {
    if (!engine_state.session_active) {
        return NULL;
    }
    if (engine_state.current_question >= engine_state.total_questions) {
        return NULL;
    }
    return duplicate_string(kQuestionBank[engine_state.current_question]);
}

char *feedback(const char *answer_json) {
    (void)answer_json;
    if (!engine_state.session_active) {
        return duplicate_string("{\"correct\":false,\"scoreDelta\":0,\"message\":\"No active session\",\"questionId\":\"\"}");
    }
    char question_id_buffer[4] = {0};
    snprintf(question_id_buffer, sizeof(question_id_buffer), "q%d", engine_state.current_question + 1);
    char *response = format_string(
        "{\"questionId\":\"%s\",\"correct\":true,\"scoreDelta\":1,"
        "\"message\":\"Great job\",\"correctAnswerId\":\"A\"}",
        question_id_buffer
    );
    engine_state.current_question += 1;
    if (engine_state.current_question >= engine_state.total_questions) {
        engine_state.session_active = false;
    }
    return response;
}

char *end_session(void) {
    int questions_answered = engine_state.current_question;
    int total = engine_state.total_questions;
    if (questions_answered > total) {
        questions_answered = total;
    }
    engine_state.session_active = false;
    engine_state.current_question = 0;
    engine_state.total_questions = 0;
    return format_string(
        "{\"totalQuestions\":%d,\"correctAnswers\":%d,\"durationMs\":120000}",
        total,
        questions_answered
    );
}

void free_string(char *ptr) {
    if (ptr != NULL) {
        free(ptr);
    }
}
