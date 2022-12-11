#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <curl/curl.h>
#include "static/cJSON.h"

#define DESC_CONCAT(x, y) if (x == NULL || !strcmp(x, y)) { x = y; } else {strcat(strcat(x, ", "), y);}
typedef struct weather_period wp;

void checkArgs(int* argc, char* argv[]) {
    if (*argc != 2) {
        printf("\n\n --- ! ОШИБКА ЗАПУСКА ----\n\n"
        "Программа принимает на вход только один аргумент - название локации\n\n"
        "------ Синтаксис ------\n"
        "%s <location>\n"
        "-----------------------\n\n"
        "Пример запуска\n"
        "%s Miami \n\n"
        "--- Повторите запуск правильно ---\n\n", argv[0], argv[0]);
        exit(1);
    }
}

struct memory {
    char *response;
    size_t size;
};

struct weather_period {
    const char *name;
    char *desc;
    int tempers[2];
    int w_speeds[2];
    char *wind_d;
};

static wp *create_weather_period(const char *p_name) {
    wp *ptr;
    ptr = (wp *)malloc(sizeof(wp));
    if (!ptr) {
        puts("error memory allocation");
        exit(1);
    }
    ptr->name = p_name;
    memcpy(ptr->tempers, (int[]){0, 0}, 2);
    memcpy(ptr->w_speeds, (int[]){0, 0}, 2);
    return ptr;
}

static size_t write_response(void *data, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct memory *mem = (struct memory *)userp;

    char *ptr = realloc(mem->response, mem->size + realsize + 1);
    if(ptr == NULL)
        exit(1);  /* out of memory! */

    mem->response = ptr;
    memcpy(&(mem->response[mem->size]), data, realsize);
    mem->size += realsize;
    mem->response[mem->size] = 0;

    return realsize;
}

int main(int argc, char* argv[]) {
    checkArgs(&argc, argv);
    (void)argc;

    char* city = argv[1];

    char url[100] = "https://wttr.in/";
    strcat(strcat(url, city),"?format=j1");
    CURL *curl;
    CURLcode res;
 
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if(curl) {
        struct memory response = {0};

        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
 
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            exit(1);
        }
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        if (response_code != 200) {
            switch (response_code)
            {
            case 404:
                printf("Такой локации <%s> не найдено! Попробуйте еще раз.\n", city);
                break;
            case 500:
                printf("Внутренняя ошибка сервера wttr.in - попробуйте позже!\n");
                break;
            default:
                printf("Получена ошибка %lu от сервера wttr.in\n", response_code);
                break;
            }
            curl_easy_cleanup(curl);
            exit(1);
        }

        const cJSON *weathers = NULL;
        const cJSON *weather = NULL;
        const cJSON *two_hours = NULL;
        const cJSON *two_hour = NULL;
        const cJSON *temp = NULL;
        const cJSON *weatherDescs = NULL;
        const cJSON *weatherDesc = NULL;
        const cJSON *wdesc = NULL;
        const cJSON *windspeedKmph = NULL;
        const cJSON *winddir16Point = NULL;

        cJSON *response_json = cJSON_Parse(response.response);
        if (response_json == NULL) {
            const char *error_ptr = cJSON_GetErrorPtr();
            if (error_ptr != NULL) {
                fprintf(stderr, "Error parsing content from wttr.in before: %s\n", error_ptr);
            }
            curl_easy_cleanup(curl);
            exit(1);
        }
        free(response.response);
        weathers = cJSON_GetObjectItemCaseSensitive(response_json, "weather");

        int periods_count = 0;

        wp *morning = create_weather_period("утром");
        wp *noon = create_weather_period("днем");
        wp *evening = create_weather_period("вечером");
        wp *night = create_weather_period("ночью");

        cJSON_ArrayForEach(weather, weathers) {
            two_hours = cJSON_GetObjectItemCaseSensitive(weather, "hourly");
            cJSON_ArrayForEach(two_hour, two_hours) {
                temp = cJSON_GetObjectItemCaseSensitive(two_hour, "DewPointC");
                windspeedKmph = cJSON_GetObjectItemCaseSensitive(two_hour, "windspeedKmph");
                winddir16Point = cJSON_GetObjectItemCaseSensitive(two_hour, "winddir16Point");
                if (periods_count == 0 || periods_count == 1) {
                    morning->tempers[periods_count % 2] = atoi(temp->valuestring);
                    morning->w_speeds[periods_count % 2] = atoi(windspeedKmph->valuestring);
                    DESC_CONCAT(morning->wind_d, winddir16Point->valuestring);
                    weatherDescs = cJSON_GetObjectItemCaseSensitive(two_hour, "weatherDesc");
                    cJSON_ArrayForEach(weatherDesc, weatherDescs) {
                        wdesc = cJSON_GetObjectItemCaseSensitive(weatherDesc, "value");
                        DESC_CONCAT(morning->desc, wdesc->valuestring);
                    }
                } 
                else if (periods_count == 2 || periods_count == 3) {
                    noon->tempers[periods_count % 2] = atoi(temp->valuestring);
                    noon->w_speeds[periods_count % 2] = atoi(windspeedKmph->valuestring);
                    DESC_CONCAT(noon->wind_d, winddir16Point->valuestring);
                    weatherDescs = cJSON_GetObjectItemCaseSensitive(two_hour, "weatherDesc");
                    cJSON_ArrayForEach(weatherDesc, weatherDescs) {
                        wdesc = cJSON_GetObjectItemCaseSensitive(weatherDesc, "value");
                        DESC_CONCAT(noon->desc, wdesc->valuestring);
                    }
                }
                else if (periods_count == 4 || periods_count == 5) {
                    evening->tempers[periods_count % 2] = atoi(temp->valuestring);
                    evening->w_speeds[periods_count % 2] = atoi(windspeedKmph->valuestring);
                    DESC_CONCAT(evening->wind_d, winddir16Point->valuestring);
                    weatherDescs = cJSON_GetObjectItemCaseSensitive(two_hour, "weatherDesc");
                    cJSON_ArrayForEach(weatherDesc, weatherDescs) {
                        wdesc = cJSON_GetObjectItemCaseSensitive(weatherDesc, "value");
                        DESC_CONCAT(evening->desc, wdesc->valuestring);
                    }
                }
                else if (periods_count == 6 || periods_count == 7) {
                    night->tempers[periods_count % 2] = atoi(temp->valuestring);
                    night->w_speeds[periods_count % 2] = atoi(windspeedKmph->valuestring);
                    DESC_CONCAT(night->wind_d, winddir16Point->valuestring);
                    weatherDescs = cJSON_GetObjectItemCaseSensitive(two_hour, "weatherDesc");
                    cJSON_ArrayForEach(weatherDesc, weatherDescs) {
                        wdesc = cJSON_GetObjectItemCaseSensitive(weatherDesc, "value");
                        DESC_CONCAT(night->desc, wdesc->valuestring);
                    }
                }
                periods_count++;
            }
        }
        puts("---------------------------------------------------");
        printf("Прогноз погоды на сегодня в городе %s:\n", city);
        puts("---------------------------------------------------");
        printf("\t%s %d°C. Ветер %.1f м/с (направ. %s). %s\n", morning->name, (morning->tempers[0] + morning->tempers[1]) / 2, ((morning->w_speeds[0] + morning->w_speeds[1]) / 2.0) * 0.278, morning->wind_d, morning->desc);
        printf("\t%s %d°C. Ветер %.1f м/с (направ. %s). %s\n", noon->name, (noon->tempers[0] + noon->tempers[1]) / 2, ((noon->w_speeds[0] + noon->w_speeds[1]) / 2.0) * 0.278, noon->wind_d, noon->desc);
        printf("\t%s %d°C. Ветер %.1f м/с (направ. %s). %s\n", evening->name, (evening->tempers[0] + evening->tempers[1]) / 2, ((evening->w_speeds[0] + evening->w_speeds[1]) / 2.0) * 0.278, evening->wind_d, evening->desc);
        printf("\t%s %d°C. Ветер %.1f м/с (направ. %s). %s\n", night->name, (night->tempers[0] + night->tempers[1]) / 2, ((night->w_speeds[0] + night->w_speeds[1]) / 2.0) * 0.278, night->wind_d, night->desc);

        puts("---------------------------------------------------");
        puts("По данным сервиса wttr.in");
        printf("Проверить по адресу %s\n", url);
        puts("---------------------------------------------------");
        free(morning);
        free(noon);
        free(evening);
        free(night);
        curl_easy_cleanup(curl);
  }
 
  curl_global_cleanup();

  return 0;
}
