#include "pill_web/schedule_json.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

bool schedule_json_parse(const char *json, size_t length, schedule_json_request_t *request)
{
    if (json == NULL || request == NULL || length == 0 || length > SCHEDULE_JSON_BODY_MAX) {
        return false;
    }
    char *buffer = malloc(length + 1);
    if (buffer == NULL) return false;
    memcpy(buffer, json, length);
    buffer[length] = '\0';
    cJSON *root = cJSON_ParseWithLength(buffer, length + 1);
    free(buffer);
    if (!cJSON_IsObject(root) ||
        (cJSON_GetArraySize(root) != 3 && cJSON_GetArraySize(root) != 4)) {
        cJSON_Delete(root);
        return false;
    }
    cJSON *medication = cJSON_GetObjectItemCaseSensitive(root, "medication");
    cJSON *dose = cJSON_GetObjectItemCaseSensitive(root, "dose_instruction");
    cJSON *times = cJSON_GetObjectItemCaseSensitive(root, "times");
    cJSON *layer = cJSON_GetObjectItemCaseSensitive(root, "layer");
    if (!cJSON_IsString(medication) || !cJSON_IsString(dose) ||
        !cJSON_IsArray(times) ||
        (layer != NULL && (!cJSON_IsNumber(layer) ||
                           layer->valuedouble != 1.0))) {
        cJSON_Delete(root);
        return false;
    }
    size_t medication_length = strlen(medication->valuestring);
    size_t dose_length = strlen(dose->valuestring);
    int count = cJSON_GetArraySize(times);
    if (medication_length == 0 || medication_length >= sizeof(request->medication) ||
        dose_length == 0 || dose_length >= sizeof(request->dose_instruction) ||
        count < 1 || count > PILL_SCHEDULE_TIMES_MAX) {
        cJSON_Delete(root);
        return false;
    }
    schedule_json_request_t parsed = {0};
    memcpy(parsed.medication, medication->valuestring, medication_length + 1);
    memcpy(parsed.dose_instruction, dose->valuestring, dose_length + 1);
    parsed.layer = 1;
    parsed.time_count = (uint8_t)count;
    for (int index = 0; index < count; ++index) {
        cJSON *time = cJSON_GetArrayItem(times, index);
        if (!cJSON_IsNumber(time) || time->valuedouble < 0 || time->valuedouble >= 1440 ||
            floor(time->valuedouble) != time->valuedouble) {
            cJSON_Delete(root);
            return false;
        }
        parsed.times[index] = (uint16_t)time->valueint;
    }
    cJSON_Delete(root);
    *request = parsed;
    return true;
}
