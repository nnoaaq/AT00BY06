#include <stdio.h>
#include <string.h>
#include "measurements.h"
#include <time.h>

int create_json_dht(char *buffer, size_t buffer_size, const char *sensor_id)
{
    int written = 0;
    int n_chars;
    n_chars = snprintf(buffer + written, buffer_size - written, "[" );
    if (n_chars < 0 || (size_t)n_chars >= buffer_size - written) return -1;
    written += n_chars;

    int item_count = 0;
    for (int i = 0; i < measurement_count; i++)
    {
        // DHT22: lämpötila ja kosteus
        if (item_count > 0)
        {
            n_chars = snprintf(buffer + written, buffer_size - written, ",");
            if (n_chars < 0 || (size_t)n_chars >= buffer_size - written)
                return -1;
            written += n_chars;
        }
        n_chars = snprintf(buffer + written, buffer_size - written,
                           "{\n"
                           "\"sensorId\":\"%s\"," 
                           "\"sensorName\":\"DHT22\"," 
                           "\"temperature\":%.1f,\n"
                           "\"humidity\":%.1f,\n"
                           "\"timeStamp\":%lld\n"
                           "}",
                           sensor_id,
                           measurements[i].dht_temperature,
                           measurements[i].humidity, (long long)measurements[i].timestamp);

        if (n_chars < 0 || (size_t)n_chars >= buffer_size - written)
            return -1;
        written += n_chars;
        item_count++;
    }
    n_chars = snprintf(buffer + written, buffer_size - written, "]");
    if (n_chars < 0 || (size_t)n_chars >= buffer_size - written) return -1;
    written += n_chars;

    return written;
}
