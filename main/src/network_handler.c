/* esp-idf-telegram-bot
 *
 * Author: antusystem
 * e-mail: aleantunes95@gmail.com
 * Date: 11-01-2020
 * MIT License
 * As it is described in the readme file
 *
*/

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "driver/gpio.h"
#include "freertos/event_groups.h"
#include "api_constants.h"

/* FreeRTOS event group to signal when we are connected*/
EventGroupHandle_t s_wifi_event_group;

void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG1, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG1,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG1, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void) {
    ESP_LOGI(TAG1, "ESP_WIFI_MODE_STA");
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ESP_WIFI_SSID,
            .password = ESP_WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG1, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG1, "connected to ap SSID:%s password:%s",
                 ESP_WIFI_SSID, ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG1, "Failed to connect to SSID:%s, password:%s",
                 ESP_WIFI_SSID, ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG1, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}


esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    static char *output_buffer;  // Buffer to store response of http request from event handler
    static int output_len;       // Stores number of bytes read
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            /*
             *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             *  However, event handler can also be used in case chunked encoding is used.
             */
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // If user_data buffer is configured, copy the response into the buffer
                if (evt->user_data) {
                    memcpy(evt->user_data + output_len, evt->data, evt->data_len);
                } else {
                    if (output_buffer == NULL) {
                        output_buffer = (char *) malloc(esp_http_client_get_content_length(evt->client));
                        output_len = 0;
                        if (output_buffer == NULL) {
                            ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                            return ESP_FAIL;
                        }
                    }
                    memcpy(output_buffer + output_len, evt->data, evt->data_len);
                }
                output_len += evt->data_len;
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (output_buffer != NULL) {
                // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
                // ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                if (output_buffer != NULL) {
                    free(output_buffer);
                    output_buffer = NULL;
                }
                output_len = 0;
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            break;
    }
    return ESP_OK;
}


/*
 *  http_native_request() demonstrates use of low level APIs to connect to a server,
 *  make a http request and read response. Event handler is not used in this case.
 *  Note: This approach should only be used in case use of low level APIs is required.
 *  The easiest way is to use esp_http_perform()
 */
void http_native_request(void) {
    char output_buffer[MAX_HTTP_OUTPUT_BUFFER] = "";   // Buffer to store response of http request
    int content_length = 0;
    esp_http_client_config_t config = {
        .url = "http://httpbin.org/get",
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // GET Request
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    } else {
        content_length = esp_http_client_fetch_headers(client);
        if (content_length < 0) {
            ESP_LOGE(TAG, "HTTP client fetch headers failed");
        } else {
            int data_read = esp_http_client_read_response(client, output_buffer, MAX_HTTP_OUTPUT_BUFFER);
            if (data_read >= 0) {
                ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
                //ESP_LOG_BUFFER_HEX(TAG, output_buffer, strlen(output_buffer));
                for(int i = 0; i < esp_http_client_get_content_length(client); i++) {
                    putchar(output_buffer[i]);
                }
                putchar('\r');
                putchar('\n');
            } else {
                ESP_LOGE(TAG, "Failed to read response");
            }
        }
    }
    esp_http_client_close(client);

    // POST Request
    const char *post_data = "{\"field1\":\"value1\"}";
    esp_http_client_set_url(client, "http://httpbin.org/post");
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    err = esp_http_client_open(client, strlen(post_data));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    } else {
        int wlen = esp_http_client_write(client, post_data, strlen(post_data));
        if (wlen < 0) {
            ESP_LOGE(TAG, "Write failed");
        }
        int data_read = esp_http_client_read_response(client, output_buffer, MAX_HTTP_OUTPUT_BUFFER);
        if (data_read >= 0) {
            ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
            esp_http_client_get_status_code(client),
            esp_http_client_get_content_length(client));
            //ESP_LOG_BUFFER_HEX(TAG, output_buffer, strlen(output_buffer));
            for(int i = 0; i < esp_http_client_get_content_length(client); i++) {
                putchar(output_buffer[i]);
            }
            putchar('\r');
            putchar('\n');
        } else {
            ESP_LOGE(TAG, "Failed to read response");
        }
    }
    esp_http_client_cleanup(client);
}

void https_telegram_getMe_perform(void) {
	char buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};   // Buffer to store response of http request
	char url[512] = "";
    esp_http_client_config_t config = {
        .url = "https://api.telegram.org",
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .event_handler = _http_event_handler,
        .cert_pem = telegram_certificate_pem_start,
        .user_data = buffer,        // Pass address of local buffer to get response
    };
    /* Creating the string of the url*/
    //Copy the url+TOKEN
    strcat(url,url_string);
    //Adding the method
    strcat(url,"/getMe");
    //ESP_LOGW(TAG2, "url es: %s",url);
    //ESP_LOGW(TAG, "Iniciare");
    esp_http_client_handle_t client = esp_http_client_init(&config);
    //You set the real url for the request
    esp_http_client_set_url(client, url);
    //ESP_LOGW(TAG, "Selecting the http method");
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    //ESP_LOGW(TAG, "Perform");
    esp_err_t err = esp_http_client_perform(client);

    //ESP_LOGW(TAG, "Revisare");
    if (err == ESP_OK) {
        ESP_LOGI(TAG2, "HTTPS Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
        ESP_LOGW(TAG2, "Desde Perform el output es: %s",buffer);
    } else {
        ESP_LOGE(TAG2, "Error perform http request %s", esp_err_to_name(err));
    }

    ESP_LOGW(TAG2, "Cerrar Cliente");
    esp_http_client_close(client);
    ESP_LOGW(TAG, "Limpiare");
    esp_http_client_cleanup(client);
}

void https_telegram_getMe_native_get(void) {

	/*	Partiendo de http_native_request
	 *  http_native_request() demonstrates use of low level APIs to connect to a server,
	 *  make a http request and read response. Event handler is not used in this case.
	 *  Note: This approach should only be used in case use of low level APIs is required.
	 *  The easiest way is to use esp_http_perform()
	 */

    char output_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};   // Buffer to store response of http request
    int content_length = 0;
    char url[512] = "";
    esp_http_client_config_t config = {
        .url = "https://api.telegram.org",
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .event_handler = _http_event_handler,
        .cert_pem = telegram_certificate_pem_start,
    };
    ESP_LOGW(TAG2, "Iniciare 2");
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // GET Request
    ESP_LOGW(TAG2, "Method");
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    ESP_LOGW(TAG2, "Open");
    /* Creating the string of the url*/
    //Copy the url+TOKEN
    strcat(url,url_string);
    //Adding the method
    strcat(url,"/getMe");
    //ESP_LOGW(TAG2, "url string es: %s",url);
    //You set the real url for the request
    esp_http_client_set_url(client, url);
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG2, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    } else {
        ESP_LOGW(TAG2, "Fetch");
        content_length = esp_http_client_fetch_headers(client);
        if (content_length < 0) {
            ESP_LOGE(TAG2, "HTTP client fetch headers failed");
        } else {
            ESP_LOGW(TAG2, "Response");
            int data_read = esp_http_client_read_response(client, output_buffer, MAX_HTTP_OUTPUT_BUFFER);
            if (data_read >= 0) {
                ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
                //ESP_LOG_BUFFER_CHAR(TAG2, output_buffer, strlen(output_buffer));
            //    ESP_LOG_BUFFER_HEX(TAG2, output_buffer, strlen(output_buffer));
                for(int i = 0; i < esp_http_client_get_content_length(client); i++) {
                    putchar(output_buffer[i]);
                }
                putchar('\r');
                putchar('\n');
            } else {
                ESP_LOGE(TAG2, "Failed to read response");
            }
        }
    }
    ESP_LOGW(TAG2, "Cerrar Cliente");
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    ESP_LOGI(TAG2, "Desde perform esp_get_free_heap_size: %d", esp_get_free_heap_size ());
}

void https_telegram_sendMessage_native_get(void) {


	/* Format for sending messages
	https://api.telegram.org/bot[BOT_TOKEN]/sendMessage?chat_id=[CHANNEL_NAME]&text=[MESSAGE_TEXT]

	For public groups you can use
	https://api.telegram.org/bot[BOT_TOKEN]/sendMessage?chat_id=@GroupName&text=hello%20world
	For private groups you have to use the chat id (which also works with public groups)
	https://api.telegram.org/bot[BOT_TOKEN]/sendMessage?chat_id=-1234567890123&text=hello%20world

	You can add your chat_id or group name, your api key and use your browser to send those messages
	The %20 is the hexa for the space
	*/

    char output_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};   // Buffer to store response of http request
    int content_length = 0;
    char url[512] = "";
    esp_http_client_config_t config = {
        .url = "https://api.telegram.org",
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .event_handler = _http_event_handler,
        .cert_pem = telegram_certificate_pem_start,
    };

    ESP_LOGW(TAG3, "Iniciare");
    esp_http_client_handle_t client = esp_http_client_init(&config);
    ESP_LOGW(TAG3, "Enviare un mensaje a un chat");
    ESP_LOGW(TAG3, "Open");
    /* Creating the string of the url*/
    //Copy the url+TOKEN
    strcat(url,url_string);
    //Then you concatenate the method with the information
    strcat(url,"/sendMessage?chat_id=");
    strcat(url,chat_ID1);
    /* Now you add the text*/
    strcat(url,"&text=");
    //Between every word you have to put %20 for the space (maybe there is another way for this)
    strcat(url,"Text%20to%20send%20to%20the%20chat");
    ESP_LOGW(TAG3, "url string es: %s",url);
    //You set the real url for the request
    esp_http_client_set_url(client, url);

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG3, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    } else {
        ESP_LOGW(TAG3, "Fetch 2");
        content_length = esp_http_client_fetch_headers(client);
        if (content_length < 0) {
            ESP_LOGE(TAG3, "HTTP client fetch headers failed");
        } else {
            ESP_LOGW(TAG3, "Response 2");
            int data_read = esp_http_client_read_response(client, output_buffer, MAX_HTTP_OUTPUT_BUFFER);
            if (data_read >= 0) {
                ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
                //ESP_LOG_BUFFER_CHAR(TAG2, output_buffer, 188);
                //ESP_LOG_BUFFER_HEX(TAG2, output_buffer, strlen(output_buffer));
                for(int i = 0; i < esp_http_client_get_content_length(client); i++) {
                    putchar(output_buffer[i]);
                }
                putchar('\r');
                putchar('\n');
            } else {
                ESP_LOGE(TAG3, "Failed to read response");
            }
        }
    }
    ESP_LOGW(TAG, "Limpiare");
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    ESP_LOGI(TAG, "esp_get_free_heap_size: %d", esp_get_free_heap_size ());
}

void https_telegram_sendMessage_perform_post(void) {


	/* Format for sending messages
	https://api.telegram.org/bot[BOT_TOKEN]/sendMessage?chat_id=[CHANNEL_NAME]&text=[MESSAGE_TEXT]

	For public groups you can use
	https://api.telegram.org/bot[BOT_TOKEN]/sendMessage?chat_id=@GroupName&text=hello%20world
	For private groups you have to use the chat id (which also works with public groups)
	https://api.telegram.org/bot[BOT_TOKEN]/sendMessage?chat_id=-1234567890123&text=hello%20world

	You can add your chat_id or group name, your api key and use your browser to send those messages
	The %20 is the hexa for the space

	The format for the json is: {"chat_id":852596694,"text":"Message using post"}
	*/

	char url[512] = "";
    char output_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};   // Buffer to store response of http request
    esp_http_client_config_t config = {
        .url = "https://api.telegram.org",
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .event_handler = _http_event_handler,
        .cert_pem = telegram_certificate_pem_start,
		.user_data = output_buffer,
    };
    //POST
    ESP_LOGW(TAG3, "Iniciare");
    esp_http_client_handle_t client = esp_http_client_init(&config);

    /* Creating the string of the url*/
    //Copy the url+TOKEN
    strcat(url,url_string);
    //Passing the method
    strcat(url,"/sendMessage");
    //ESP_LOGW(TAG3, "url string es: %s",url);
    //You set the real url for the request
    esp_http_client_set_url(client, url);


	ESP_LOGW(TAG3, "Enviare POST");
	/*Here you add the text and the chat id
	 * The format for the json for the telegram request is: {"chat_id":123456789,"text":"Here goes the message"}
	  */
	// The example had this, but to add the chat id easierly I decided not to use a pointer
	//const char *post_data = "{\"chat_id\":852596694,\"text\":\"Envio de post\"}";
	char post_data[512] = "";
	sprintf(post_data,"{\"chat_id\":%s,\"text\":\"Here goes the message from post\"}",chat_ID2);
    //ESP_LOGW(TAG, "El json es es: %s",post_data);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG3, "HTTP POST Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
        ESP_LOGW(TAG3, "Desde Perform el output es: %s",output_buffer);

    } else {
        ESP_LOGE(TAG3, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    ESP_LOGW(TAG, "Limpiare");
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    ESP_LOGI(TAG3, "esp_get_free_heap_size: %d", esp_get_free_heap_size ());
}


void http_test_task(void *pvParameters) {
    /* Creating the string of the url*/
    // You concatenate the host with the Token so you only have to write the method
	strcat(url_string,TOKEN);
    ESP_LOGW(TAG, "Wait 2 second before start");
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGW(TAG, "https_telegram_getMe_perform");
    https_telegram_getMe_perform();
    /* The functions https_telegram_getMe_native_get and https_telegram_sendMessage_native_get usually reboot the esp32 at when you use it after another and
     *  the second one finish, but I don't know why. Either way, it still send the message and obtain the getMe response, but the perform way is better
     *  for both options, especially for sending message with Json.*/
    //ESP_LOGW(TAG, "https_telegram_getMe_native_get");
    //https_telegram_getMe_native_get();
    //ESP_LOGW(TAG, "https_telegram_sendMessage_native_get");
    //https_telegram_sendMessage_native_get();
    ESP_LOGW(TAG, "https_telegram_sendMessage_perform_post");
    https_telegram_sendMessage_perform_post();

    ESP_LOGI(TAG, "Finish http example");
    vTaskDelete(NULL);
}