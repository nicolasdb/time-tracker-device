/**
 * @file dns_server.c
 * @brief Simple DNS server for captive portal
 * 
 * Responds to all DNS queries with the AP IP address (192.168.4.1)
 * to force browsers to connect to the configuration interface.
 */

#include "dns_server.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <string.h>

static const char *TAG = "DNS_SERVER";

#define DNS_PORT 53
#define DNS_BUFFER_SIZE 512
#define DNS_RESPONSE_TIMEOUT_MS 1000

// DNS Header structure
typedef struct {
    uint16_t id;        // Transaction ID
    uint16_t flags;     // Flags
    uint16_t qdcount;   // Questions count
    uint16_t ancount;   // Answers count
    uint16_t nscount;   // Authority records count
    uint16_t arcount;   // Additional records count
} __attribute__((packed)) dns_header_t;

// DNS server context
struct dns_server_context {
    int socket_fd;
    bool running;
    TaskHandle_t task_handle;
    uint32_t ap_ip;  // AP IP in network byte order
};

static struct dns_server_context dns_ctx = {0};

// Forward declarations
static void dns_server_task(void *pvParameters);
static void process_dns_query(int socket_fd, struct sockaddr_in *client_addr, uint8_t *buffer, int len);
static int create_dns_response(uint8_t *query_buffer, int query_len, uint8_t *response_buffer, uint32_t ap_ip);

esp_err_t dns_server_start(uint32_t ap_ip)
{
    if (dns_ctx.running) {
        ESP_LOGW(TAG, "DNS server already running");
        return ESP_OK;
    }
    
    dns_ctx.ap_ip = ap_ip;
    
    // Create UDP socket
    dns_ctx.socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (dns_ctx.socket_fd < 0) {
        ESP_LOGE(TAG, "Failed to create socket: errno %d", errno);
        return ESP_FAIL;
    }
    
    // Enable socket reuse
    int reuse = 1;
    if (setsockopt(dns_ctx.socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        ESP_LOGW(TAG, "Failed to set SO_REUSEADDR: errno %d", errno);
    }
    
    // Bind to DNS port
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(DNS_PORT);
    
    if (bind(dns_ctx.socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind socket: errno %d", errno);
        close(dns_ctx.socket_fd);
        return ESP_FAIL;
    }
    
    // Set socket timeout
    struct timeval timeout = {
        .tv_sec = DNS_RESPONSE_TIMEOUT_MS / 1000,
        .tv_usec = (DNS_RESPONSE_TIMEOUT_MS % 1000) * 1000
    };
    setsockopt(dns_ctx.socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    // Create DNS server task
    BaseType_t ret = xTaskCreate(
        dns_server_task,
        "dns_server",
        4096,  // Stack size
        NULL,
        5,     // Priority
        &dns_ctx.task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create DNS server task");
        close(dns_ctx.socket_fd);
        return ESP_FAIL;
    }
    
    dns_ctx.running = true;
    ESP_LOGI(TAG, "✅ DNS server started on port %d", DNS_PORT);
    
    return ESP_OK;
}

esp_err_t dns_server_stop(void)
{
    if (!dns_ctx.running) {
        return ESP_OK;
    }
    
    dns_ctx.running = false;
    
    // Close socket to unblock the task
    if (dns_ctx.socket_fd >= 0) {
        close(dns_ctx.socket_fd);
        dns_ctx.socket_fd = -1;
    }
    
    // Wait for task to finish
    if (dns_ctx.task_handle) {
        vTaskDelete(dns_ctx.task_handle);
        dns_ctx.task_handle = NULL;
    }
    
    ESP_LOGI(TAG, "DNS server stopped");
    return ESP_OK;
}

bool dns_server_is_running(void)
{
    return dns_ctx.running;
}

static void dns_server_task(void *pvParameters)
{
    uint8_t buffer[DNS_BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    
    ESP_LOGI(TAG, "DNS server task started");
    
    while (dns_ctx.running) {
        int len = recvfrom(dns_ctx.socket_fd, buffer, sizeof(buffer), 0, 
                          (struct sockaddr*)&client_addr, &client_addr_len);
        
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout - continue loop
                continue;
            }
            if (dns_ctx.running) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            }
            break;
        }
        
        if (len > 0) {
            ESP_LOGD(TAG, "Received DNS query from %s:%d (%d bytes)", 
                     inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), len);
            process_dns_query(dns_ctx.socket_fd, &client_addr, buffer, len);
        }
    }
    
    ESP_LOGI(TAG, "DNS server task finished");
    vTaskDelete(NULL);
}

static void process_dns_query(int socket_fd, struct sockaddr_in *client_addr, uint8_t *buffer, int len)
{
    // Minimum DNS query size check
    if (len < sizeof(dns_header_t)) {
        ESP_LOGW(TAG, "DNS query too short");
        return;
    }
    
    dns_header_t *header = (dns_header_t*)buffer;
    
    // Only process standard queries
    if ((ntohs(header->flags) & 0x8000) != 0) {
        ESP_LOGD(TAG, "Ignoring DNS response packet");
        return;
    }
    
    // Create response
    uint8_t response[DNS_BUFFER_SIZE];
    int response_len = create_dns_response(buffer, len, response, dns_ctx.ap_ip);
    
    if (response_len > 0) {
        int sent = sendto(socket_fd, response, response_len, 0, 
                         (struct sockaddr*)client_addr, sizeof(*client_addr));
        if (sent < 0) {
            ESP_LOGW(TAG, "Failed to send DNS response: errno %d", errno);
        } else {
            ESP_LOGD(TAG, "Sent DNS response (%d bytes) to %s:%d", 
                     sent, inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
        }
    }
}

static int create_dns_response(uint8_t *query_buffer, int query_len, uint8_t *response_buffer, uint32_t ap_ip)
{
    // Copy query to response buffer
    memcpy(response_buffer, query_buffer, query_len);
    
    dns_header_t *header = (dns_header_t*)response_buffer;
    
    // Modify header for response
    header->flags = htons(0x8180);  // Response, no error
    header->ancount = header->qdcount;  // Same number of answers as questions
    
    // Find end of questions section
    uint8_t *pos = response_buffer + sizeof(dns_header_t);
    uint16_t questions = ntohs(header->qdcount);
    
    // Skip through questions
    for (int i = 0; i < questions && pos < response_buffer + query_len; i++) {
        // Skip domain name
        while (pos < response_buffer + query_len && *pos != 0) {
            if ((*pos & 0xC0) == 0xC0) {
                // Compressed name - skip 2 bytes
                pos += 2;
                break;
            } else {
                // Regular label - skip label length + data
                pos += *pos + 1;
            }
        }
        if (pos < response_buffer + query_len && *pos == 0) {
            pos++;  // Skip null terminator
        }
        pos += 4;  // Skip QTYPE and QCLASS
    }
    
    // Add answers - point all queries to AP IP
    for (int i = 0; i < questions; i++) {
        // Name (compressed pointer to original question)
        *pos++ = 0xC0;  // Compression flag
        *pos++ = 0x0C;  // Offset to first question (after header)
        
        // Type (A record)
        *pos++ = 0x00;
        *pos++ = 0x01;
        
        // Class (IN)
        *pos++ = 0x00;
        *pos++ = 0x01;
        
        // TTL (60 seconds)
        *pos++ = 0x00;
        *pos++ = 0x00;
        *pos++ = 0x00;
        *pos++ = 0x3C;
        
        // Data length (4 bytes for IPv4)
        *pos++ = 0x00;
        *pos++ = 0x04;
        
        // IP address (AP IP in network byte order)
        memcpy(pos, &ap_ip, 4);
        pos += 4;
    }
    
    int response_len = pos - response_buffer;
    
    // Ensure we don't exceed buffer size
    if (response_len > DNS_BUFFER_SIZE) {
        ESP_LOGE(TAG, "DNS response too large");
        return 0;
    }
    
    return response_len;
}