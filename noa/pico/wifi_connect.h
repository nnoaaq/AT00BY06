typedef enum
{
    WIFI_IDLE,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    WIFI_FAILED,
} wifi_state_t;

bool wifi_connect_init();
bool wifi_connect_start();
wifi_state_t wifi_connect_poll();
bool wifi_is_connected();
