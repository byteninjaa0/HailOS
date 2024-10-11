#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Check if the compiler thinks you are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "This tutorial needs to be compiled with a ix86-elf compiler"
#endif

/* Hardware text mode color constants. */
enum vga_color
{
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
};

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t *terminal_buffer;

#define MAX_CMD_LENGTH 256

// Function declarations
static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg);
static inline uint16_t vga_entry(unsigned char uc, uint8_t color);
size_t strlen(const char *str);
static inline void outb(uint16_t port, uint8_t val);
static inline uint8_t inb(uint16_t port);
void terminal_initialize(void);
void terminal_setcolor(uint8_t color);
void terminal_putentryat(char c, uint8_t color, size_t x, size_t y);
void terminal_scroll(void);
void terminal_move_cursor(size_t row, size_t col);
void terminal_update_cursor(void);
void terminal_putchar(char c);
void terminal_write(const char *data, size_t size);
void terminal_writestring(const char *data);
void terminal_writestring_color(const char *data, uint8_t color);
uint8_t scan(void);
char scancode_to_ascii(uint8_t scancode);
void readline(char *buffer, size_t buffer_size);
void cmd_help(void);
void cmd_clear(void);
void cmd_color(const char *arg);
void execute_command(char *cmd);
int strcmp(const char *s1, const char *s2);
uint8_t read_rtc(uint8_t reg);
void cmd_time(void);
void cmd_date(void);
void itoa(int value, char* str, int base);

void itoa(int value, char* str, int base) {
    char* rc;
    char* ptr;
    char* low;
    // Set '-' for negative decimals
    if (base == 10 && value < 0) {
        *str++ = '-';
        value = -value;
    }

    rc = ptr = str;

    // Convert the number to the given base
    do {
        *ptr++ = "0123456789ABCDEF"[value % base];
        value /= base;
    } while (value);

    // Terminate the string
    *ptr-- = '\0';

    // Reverse the string
    for (low = rc; low < ptr; low++, ptr--) {
        char temp = *low;
        *low = *ptr;
        *ptr = temp;
    }
}

uint8_t read_rtc(uint8_t reg)
{
    outb(0x70, reg);
    return inb(0x71);
}

void cmd_time()
{
    uint8_t hours = read_rtc(0x04);
    uint8_t minutes = read_rtc(0x02);
    uint8_t seconds = read_rtc(0x00);
    char buffer[9];
    
    itoa(hours, buffer, 16);  // Convert hours to hex string
    terminal_writestring(buffer);
    terminal_writestring(":");
    
    itoa(minutes, buffer, 16);  // Convert minutes to hex string
    terminal_writestring(buffer);
    terminal_writestring(":");
    
    itoa(seconds, buffer, 16);  // Convert seconds to hex string
    terminal_writestring(buffer);
    terminal_writestring("\n");
}

void cmd_date()
{
    uint8_t day = read_rtc(0x07);
    uint8_t month = read_rtc(0x08);
    uint8_t year = read_rtc(0x09);
    char buffer[5];  // Buffer for each component (day, month, year)

    terminal_writestring("Current date: ");

    // Convert day to a string
    itoa(day, buffer, 16);
    if (day < 10) terminal_putchar('0');  // Add leading zero if needed
    terminal_writestring(buffer);
    terminal_writestring("/");

    // Convert month to a string
    itoa(month, buffer, 16);
    if (month < 10) terminal_putchar('0');  // Add leading zero if needed
    terminal_writestring(buffer);
    terminal_writestring("/");

    // Convert year to a string
    itoa(year, buffer, 16);
    if (year < 10) terminal_putchar('0');  // Add leading zero if needed
    terminal_writestring(buffer);

    terminal_writestring("\n");
}

// Function implementations
static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg)
{
    return fg | bg << 4;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color)
{
    return (uint16_t)uc | (uint16_t)color << 8;
}

size_t strlen(const char *str)
{
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    asm volatile("inb %1, %0"
                 : "=a"(ret)
                 : "Nd"(port));
    return ret;
}

void terminal_initialize(void)
{
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_buffer = (uint16_t *)0xB8000;
    for (size_t y = 0; y < VGA_HEIGHT; y++)
    {
        for (size_t x = 0; x < VGA_WIDTH; x++)
        {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
}

void terminal_setcolor(uint8_t color)
{
    terminal_color = color;
}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y)
{
    const size_t index = y * VGA_WIDTH + x;
    terminal_buffer[index] = vga_entry(c, color);
}

void terminal_scroll(void)
{
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++)
    {
        for (size_t x = 0; x < VGA_WIDTH; x++)
        {
            terminal_buffer[y * VGA_WIDTH + x] = terminal_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }
    for (size_t x = 0; x < VGA_WIDTH; x++)
    {
        terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
    }
}

void terminal_move_cursor(size_t row, size_t col)
{
    uint16_t position = (row * VGA_WIDTH) + col;
    // Cursor location I/O ports
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(position & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((position >> 8) & 0xFF));
}

void terminal_update_cursor(void)
{
    terminal_move_cursor(terminal_row, terminal_column);
}

void terminal_putchar(char c)
{
    if (c == '\n')
    {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT)
        {
            terminal_row = VGA_HEIGHT - 1;
            terminal_scroll();
        }
    }
    else
    {
        terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
        if (++terminal_column == VGA_WIDTH)
        {
            terminal_column = 0;
            if (++terminal_row == VGA_HEIGHT)
            {
                terminal_row = VGA_HEIGHT - 1;
                terminal_scroll();
            }
        }
    }
    terminal_update_cursor();
}

void terminal_write(const char *data, size_t size)
{
    for (size_t i = 0; i < size; i++)
        terminal_putchar(data[i]);
}

void terminal_writestring(const char *data)
{
    terminal_write(data, strlen(data));
}

void terminal_writestring_color(const char *data, uint8_t color)
{
    uint8_t old_color = terminal_color;
    terminal_setcolor(color);
    terminal_writestring(data);
    terminal_setcolor(old_color);
}

uint8_t scan(void)
{
    uint8_t scancode;
    // Wait for a scancode to be available
    while (!(inb(0x64) & 1))
        ;
    // Read the scancode
    scancode = inb(0x60);
    return scancode;
}

char scancode_to_ascii(uint8_t scancode)
{
    const char scancode_to_ascii[] = {
        0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
        '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '};
    if (scancode < sizeof(scancode_to_ascii))
    {
        return scancode_to_ascii[scancode];
    }
    return 0;
}

void readline(char *buffer, size_t buffer_size)
{
    size_t i = 0;
    while (i < buffer_size - 1)
    {
        uint8_t scancode = scan();
        if (scancode & 0x80) // Ignore key release events
        {
            continue;
        }
        
        char c = scancode_to_ascii(scancode);
        
        if (c == '\n')  // Enter key
        {
            buffer[i] = '\0'; // Null-terminate the input
            terminal_putchar('\n'); // Move to the next line
            break; // Exit the loop
        }
        else if (c == 0) // Invalid character (like scancode 0)
        {
            continue; // Ignore
        }
        else if (scancode == 0x0E) // Backspace key (scancode for backspace)
        {
            if (i > 0) // Only delete if there is something to delete
            {
                i--; // Move back in the buffer
                terminal_putchar('\b'); // Move cursor back
                terminal_putchar(' '); // Overwrite with space
                terminal_putchar('\b'); // Move cursor back again
            }
        }
        else if (c >= ' ' && c <= '~') // Valid printable character
        {
            buffer[i++] = c; // Store the character in the buffer
            terminal_putchar(c); // Print the character to the terminal
        }
    }
    buffer[i] = '\0'; // Ensure null termination
}


void cmd_help()
{
    terminal_writestring("Available commands:\n");
    terminal_writestring("  help  - Display this help message\n");
    terminal_writestring("  clear - Clear the screen\n");
    terminal_writestring("  color - Change text color (usage: color <number 0-15>)\n");
    terminal_writestring("  time  - display the current time\n");
    terminal_writestring("  date  - display the current date\n");
}

void cmd_clear()
{
    terminal_initialize();
}

void cmd_color(const char *arg)
{
    int color = arg[0] - '0';
    if (color >= 0 && color <= 15)
    {
        terminal_setcolor(vga_entry_color(color, VGA_COLOR_BLACK));
        terminal_writestring("Color changed.\n");
    }
    else
    {
        terminal_writestring("Invalid color. Please use a number between 0 and 15.\n");
    }
}

void execute_command(char *cmd)
{
    char *arg = cmd;
    while (*arg && *arg != ' ')
        arg++;
    if (*arg == ' ')
    {
        *arg = '\0';
        arg++;
    }

    if (strcmp(cmd, "help") == 0)
    {
        cmd_help();
    }
    else if (strcmp(cmd, "clear") == 0)
    {
        cmd_clear();
    }
    else if (strcmp(cmd, "time") == 0)
    {
        cmd_time();
    }
    else if (strcmp(cmd, "date") == 0)
    {
        cmd_date();
    }
    else if (strcmp(cmd, "color") == 0)
    {
        cmd_color(arg);
    }

    else
    {
        terminal_writestring("Unknown command. Type 'help' for a list of commands.\n");
    }
}

int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

void kernel_main(void)
{
    /* Initialize terminal interface */
    terminal_initialize();

    terminal_writestring_color(" #    #    ##       #    #\n", VGA_COLOR_LIGHT_GREEN);
    terminal_writestring_color(" #    #   #  #      #    #\n", VGA_COLOR_LIGHT_GREEN);
    terminal_writestring_color(" ######  #    #     #    #\n", VGA_COLOR_LIGHT_GREEN);
    terminal_writestring_color(" #    #  ######     #    #\n", VGA_COLOR_LIGHT_GREEN);
    terminal_writestring_color(" #    #  #    #     #    ######\n", VGA_COLOR_LIGHT_GREEN);
    terminal_writestring_color("          \n", VGA_COLOR_LIGHT_RED);
    terminal_writestring_color("#######  #####\n", VGA_COLOR_LIGHT_RED);
    terminal_writestring_color("#     # #     #\n", VGA_COLOR_LIGHT_RED);
    terminal_writestring_color("#     # #\n", VGA_COLOR_LIGHT_RED);
    terminal_writestring_color("#     #  #####\n", VGA_COLOR_LIGHT_RED);
    terminal_writestring_color("#     #       #\n", VGA_COLOR_LIGHT_RED);
    terminal_writestring_color("#     # #     #\n", VGA_COLOR_LIGHT_RED);
    terminal_writestring_color("#######  #####\n", VGA_COLOR_LIGHT_RED);

    terminal_writestring_color("Welcome to myos!\n", VGA_COLOR_LIGHT_GREEN);
    terminal_writestring("Type 'help' for a list of commands.\n\n");

    char cmd_buffer[MAX_CMD_LENGTH];

    while (1)
    {
        terminal_writestring("> ");
        readline(cmd_buffer, MAX_CMD_LENGTH);
        execute_command(cmd_buffer);
    }
}