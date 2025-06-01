#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

// I2C device address and bus
#define I2C_DEVICE_ADDR 0x26
#define MAX_I2C_BUS 20

// LED control registers
#define LED_ON_REG_0    0xA0
#define LED_OFF_REG_0   0xB0
#define LED_ON_REG_1    0xA1
#define LED_OFF_REG_1   0xB1

// LED bit masks
#define HDD0_WHITE      0x04
#define HDD0_RED        0x08
#define HDD1_WHITE      0x10
#define HDD1_RED        0x20
#define NETWORK_WHITE   0x40
#define NETWORK_RED     0x80
#define NVME0_WHITE     0x01
#define NVME0_RED       0x02
#define NVME1_WHITE     0x04
#define NVME1_RED       0x08
#define NVME2_WHITE     0x10
#define NVME2_RED       0x20
#define NVME3_WHITE     0x40
#define NVME3_RED       0x80

// Thresholds for activity levels
#define HIGH_UTILIZATION_THRESHOLD  80.0
#define MEDIUM_UTILIZATION_THRESHOLD 50.0
#define ACTIVITY_SAMPLE_INTERVAL    1000000  // 1 second in microseconds

typedef struct {
    char device_name[32];
    unsigned long long prev_read_sectors;
    unsigned long long prev_write_sectors;
    unsigned long long prev_read_time;
    unsigned long long prev_write_time;
    double utilization_percent;
    int is_active;
} disk_stats_t;

typedef struct {
    char interface_name[32];
    unsigned long long prev_rx_bytes;
    unsigned long long prev_tx_bytes;
    int is_active;
} network_stats_t;

// Global variables
static int i2c_fd = -1;
static int i2c_bus = -1;
static volatile int running = 1;

// Function prototypes
int find_i2c_bus(void);
int init_i2c(void);
void cleanup_i2c(void);
int write_i2c_register(int reg, int value);
void set_led_state(int reg, int mask, int state);
void update_disk_leds(disk_stats_t *disks, int num_disks);
void update_network_led(network_stats_t *network);
int read_disk_stats(disk_stats_t *disks, int num_disks);
int read_network_stats(network_stats_t *network);
void signal_handler(int signal);
void turn_off_all_leds(void);

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    printf("\nReceived signal %d, shutting down...\n", signal);
    running = 0;
}

// Find the I2C bus that has our LED controller
int find_i2c_bus(void) {
    char filename[32];
    int fd;
    
    for (int bus = 0; bus < MAX_I2C_BUS; bus++) {
        snprintf(filename, sizeof(filename), "/dev/i2c-%d", bus);
        fd = open(filename, O_RDWR);
        
        if (fd >= 0) {
            if (ioctl(fd, I2C_SLAVE, I2C_DEVICE_ADDR) >= 0) {
                // Try to read from the device using SMBus to verify it exists
                __s32 result = i2c_smbus_read_byte(fd);
                if (result >= 0 || errno == EAGAIN) {
                    close(fd);
                    printf("Found LED controller on I2C bus %d\n", bus);
                    return bus;
                }
            }
            close(fd);
        }
    }
    
    printf("LED controller not found on any I2C bus\n");
    return -1;
}

// Initialize I2C communication
int init_i2c(void) {
    char filename[32];
    
    i2c_bus = find_i2c_bus();
    if (i2c_bus < 0) {
        return -1;
    }
    
    snprintf(filename, sizeof(filename), "/dev/i2c-%d", i2c_bus);
    i2c_fd = open(filename, O_RDWR);
    
    if (i2c_fd < 0) {
        perror("Failed to open I2C device");
        return -1;
    }
    
    if (ioctl(i2c_fd, I2C_SLAVE, I2C_DEVICE_ADDR) < 0) {
        perror("Failed to set I2C slave address");
        cleanup_i2c();
        return -1;
    }
    
    printf("I2C initialized successfully on bus %d\n", i2c_bus);
    return 0;
}

// Cleanup I2C resources
void cleanup_i2c(void) {
    if (i2c_fd >= 0) {
        close(i2c_fd);
        i2c_fd = -1;
    }
}

// Write to I2C register using SMBus
int write_i2c_register(int reg, int value) {
    __s32 result = i2c_smbus_write_byte_data(i2c_fd, reg, value);
    
    if (result < 0) {
        perror("Failed to write to I2C device via SMBus");
        return -1;
    }
    
    return 0;
}

// Set LED state (on/off)
void set_led_state(int reg, int mask, int state) {
    if (state) {
        write_i2c_register(reg, mask);
    } else {
        write_i2c_register(reg + 0x10, mask);  // OFF register is +0x10 from ON register
    }
}

// Turn off all LEDs
void turn_off_all_leds(void) {
    printf("Turning off all LEDs...\n");
    
    // Turn off HDD LEDs
    write_i2c_register(LED_OFF_REG_0, HDD0_WHITE);
    write_i2c_register(LED_OFF_REG_0, HDD0_RED);
    write_i2c_register(LED_OFF_REG_0, HDD1_WHITE);
    write_i2c_register(LED_OFF_REG_0, HDD1_RED);
    write_i2c_register(LED_OFF_REG_0, NETWORK_WHITE);
    write_i2c_register(LED_OFF_REG_0, NETWORK_RED);
    
    // Turn off NVME LEDs
    write_i2c_register(LED_OFF_REG_1, NVME0_WHITE);
    write_i2c_register(LED_OFF_REG_1, NVME0_RED);
    write_i2c_register(LED_OFF_REG_1, NVME1_WHITE);
    write_i2c_register(LED_OFF_REG_1, NVME1_RED);
    write_i2c_register(LED_OFF_REG_1, NVME2_WHITE);
    write_i2c_register(LED_OFF_REG_1, NVME2_RED);
    write_i2c_register(LED_OFF_REG_1, NVME3_WHITE);
    write_i2c_register(LED_OFF_REG_1, NVME3_RED);
}

// Read disk statistics from /proc/diskstats
int read_disk_stats(disk_stats_t *disks, int num_disks) {
    FILE *fp;
    char line[256];
    unsigned int major, minor;
    char device[32];
    unsigned long long reads, reads_merged, read_sectors, read_time;
    unsigned long long writes, writes_merged, write_sectors, write_time;
    unsigned long long io_in_progress, io_time, weighted_io_time;
    
    fp = fopen("/proc/diskstats", "r");
    if (!fp) {
        perror("Failed to open /proc/diskstats");
        return -1;
    }
    
    while (fgets(line, sizeof(line), fp)) {
        int parsed = sscanf(line, "%u %u %s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                           &major, &minor, device, &reads, &reads_merged, &read_sectors, &read_time,
                           &writes, &writes_merged, &write_sectors, &write_time,
                           &io_in_progress, &io_time, &weighted_io_time);
        
        if (parsed >= 14) {
            for (int i = 0; i < num_disks; i++) {
                if (strcmp(device, disks[i].device_name) == 0) {
                    // Calculate utilization based on I/O time
                    double time_diff = (double)(io_time - disks[i].prev_write_time);
                    if (time_diff > 0) {
                        disks[i].utilization_percent = (time_diff / 10.0);  // Convert to percentage
                        if (disks[i].utilization_percent > 100.0) {
                            disks[i].utilization_percent = 100.0;
                        }
                    }
                    
                    // Check for activity (sectors read/written changed)
                    disks[i].is_active = (read_sectors != disks[i].prev_read_sectors ||
                                         write_sectors != disks[i].prev_write_sectors);
                    
                    // Update previous values
                    disks[i].prev_read_sectors = read_sectors;
                    disks[i].prev_write_sectors = write_sectors;
                    disks[i].prev_write_time = io_time;
                    
                    break;
                }
            }
        }
    }
    
    fclose(fp);
    return 0;
}

// Read network statistics from /proc/net/dev
int read_network_stats(network_stats_t *network) {
    FILE *fp;
    char line[256];
    char interface[32];
    unsigned long long rx_bytes, rx_packets, rx_errs, rx_drop, rx_fifo, rx_frame, rx_compressed, rx_multicast;
    unsigned long long tx_bytes, tx_packets, tx_errs, tx_drop, tx_fifo, tx_colls, tx_carrier, tx_compressed;
    
    fp = fopen("/proc/net/dev", "r");
    if (!fp) {
        perror("Failed to open /proc/net/dev");
        return -1;
    }
    
    // Skip header lines
    fgets(line, sizeof(line), fp);
    fgets(line, sizeof(line), fp);
    
    network->is_active = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        int parsed = sscanf(line, "%31[^:]: %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                           interface, &rx_bytes, &rx_packets, &rx_errs, &rx_drop, &rx_fifo, &rx_frame, &rx_compressed, &rx_multicast,
                           &tx_bytes, &tx_packets, &tx_errs, &tx_drop, &tx_fifo, &tx_colls, &tx_carrier, &tx_compressed);
        
        if (parsed >= 17) {
            // Skip loopback interface
            if (strncmp(interface, "lo", 2) == 0) {
                continue;
            }
            
            // Check for network activity
            if (rx_bytes != network->prev_rx_bytes || tx_bytes != network->prev_tx_bytes) {
                network->is_active = 1;
                strncpy(network->interface_name, interface, sizeof(network->interface_name) - 1);
                network->interface_name[sizeof(network->interface_name) - 1] = '\0';
            }
            
            network->prev_rx_bytes = rx_bytes;
            network->prev_tx_bytes = tx_bytes;
        }
    }
    
    fclose(fp);
    return 0;
}

// Update disk LEDs based on utilization
void update_disk_leds(disk_stats_t *disks, int num_disks) {
    for (int i = 0; i < num_disks; i++) {
        int reg, white_mask, red_mask;
        
        // Map disk to appropriate LED
        if (strcmp(disks[i].device_name, "sda") == 0) {
            reg = LED_ON_REG_0;
            white_mask = HDD0_WHITE;
            red_mask = HDD0_RED;
        } else if (strcmp(disks[i].device_name, "sdb") == 0) {
            reg = LED_ON_REG_0;
            white_mask = HDD1_WHITE;
            red_mask = HDD1_RED;
        } else if (strcmp(disks[i].device_name, "nvme0n1") == 0) {
            reg = LED_ON_REG_1;
            white_mask = NVME0_WHITE;
            red_mask = NVME0_RED;
        } else if (strcmp(disks[i].device_name, "nvme1n1") == 0) {
            reg = LED_ON_REG_1;
            white_mask = NVME1_WHITE;
            red_mask = NVME1_RED;
        } else if (strcmp(disks[i].device_name, "nvme2n1") == 0) {
            reg = LED_ON_REG_1;
            white_mask = NVME2_WHITE;
            red_mask = NVME2_RED;
        } else if (strcmp(disks[i].device_name, "nvme3n1") == 0) {
            reg = LED_ON_REG_1;
            white_mask = NVME3_WHITE;
            red_mask = NVME3_RED;
        } else {
            continue;  // Unknown disk
        }
        
        // Turn off both colors first
        set_led_state(reg, white_mask, 0);
        set_led_state(reg, red_mask, 0);
        
        // Set LED based on utilization and activity
        if (disks[i].is_active) {
            if (disks[i].utilization_percent >= HIGH_UTILIZATION_THRESHOLD) {
                // High utilization - red LED
                set_led_state(reg, red_mask, 1);
            } else if (disks[i].utilization_percent >= MEDIUM_UTILIZATION_THRESHOLD) {
                // Medium utilization - white LED
                set_led_state(reg, white_mask, 1);
            } else {
                // Low utilization but active - dim white LED
                set_led_state(reg, white_mask, 1);
            }
        }
        
        printf("Disk %s: %.1f%% utilization, %s\n", 
               disks[i].device_name, 
               disks[i].utilization_percent,
               disks[i].is_active ? "active" : "idle");
    }
}

// Update network LED based on activity
void update_network_led(network_stats_t *network) {
    // Turn off both colors first
    set_led_state(LED_ON_REG_0, NETWORK_WHITE, 0);
    set_led_state(LED_ON_REG_0, NETWORK_RED, 0);
    
    if (network->is_active) {
        // Network activity - white LED
        set_led_state(LED_ON_REG_0, NETWORK_WHITE, 1);
        printf("Network: active on %s\n", network->interface_name);
    } else {
        printf("Network: idle\n");
    }
}

int main(int argc, char *argv[]) {
    disk_stats_t disks[6] = {
        {"sda", 0, 0, 0, 0, 0.0, 0},
        {"sdb", 0, 0, 0, 0, 0.0, 0},
        {"nvme0n1", 0, 0, 0, 0, 0.0, 0},
        {"nvme1n1", 0, 0, 0, 0, 0.0, 0},
        {"nvme2n1", 0, 0, 0, 0, 0.0, 0},
        {"nvme3n1", 0, 0, 0, 0, 0.0, 0}
    };
    
    network_stats_t network = {"", 0, 0, 0};
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("LED Disk & Network Activity Monitor\n");
    printf("Press Ctrl+C to exit\n\n");
    
    // Initialize I2C
    if (init_i2c() < 0) {
        fprintf(stderr, "Failed to initialize I2C\n");
        return 1;
    }
    
    // Turn off all LEDs initially
    turn_off_all_leds();
    
    // Initialize disk stats (first read to establish baseline)
    read_disk_stats(disks, 6);
    read_network_stats(&network);
    
    printf("Starting monitoring loop...\n\n");
    
    // Main monitoring loop
    while (running) {
        // Read current stats
        if (read_disk_stats(disks, 6) < 0) {
            fprintf(stderr, "Failed to read disk stats\n");
            continue;
        }
        
        if (read_network_stats(&network) < 0) {
            fprintf(stderr, "Failed to read network stats\n");
            continue;
        }
        
        // Update LEDs
        update_disk_leds(disks, 6);
        update_network_led(&network);
        
        printf("---\n");
        
        // Wait before next iteration
        usleep(ACTIVITY_SAMPLE_INTERVAL);
    }
    
    // Cleanup
    turn_off_all_leds();
    cleanup_i2c();
    
    printf("LED monitor stopped.\n");
    return 0;
}