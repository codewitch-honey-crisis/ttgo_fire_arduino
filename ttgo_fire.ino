// spans in htcw_gfx allow direct access to the memory behind a draw target.
// this is generally a bitmap, but may be abstracted, such as by htcw_uix's
// control_surface<>. Either way, spans are used to increase performance
// by allowing for reading & writing directly to the memory instead of
// having to use callback methods which is the default.
// if USE_SPANS is enabled here, spans are used to increase performance
// otherwise traditional draw mechanisms are used.
#define USE_SPANS
#include "Arduino.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "lcd_miser.h"
#include "htcw_button.h"
#include "gfx.h"
#include "uix.h"
// the below Win 3.1 .FON file was converted to a header
// using https://honeythecodewitch.com/gfx/converter 
// With htcw_gfx 2.x you must use the C/C++ option,
// and not the GFX options, since those are for htcw_gfx 1.x
#define VGA_8X8_IMPLEMENTATION
#include "assets/vga_8x8.h"

using namespace gfx; // import htcw_gfx types
using namespace uix; // import htcw_uix types

using button_a_raw_t = arduino::basic_button;//< 0,10,true>;
using button_b_raw_t = arduino::basic_button;//<35,10,true>;
using button_t = arduino::button;
button_a_raw_t button_a_raw(0,10,true);
button_b_raw_t button_b_raw(35,10,true);
button_t& button_a = button_a_raw;
button_t& button_b = button_b_raw;

using lcd_miser_t = htcw::lcd_miser<4>;
lcd_miser_t lcd_light;

// make a transfer buffer big enough for 1/10th to 1/2 of the display.
// depending on draw time and display size you may want to tweak this
// (already pretweaked for this demo)
static constexpr const size_t lcd_transfer_size = 240 * 135 * 2 / 2;
// transfer buffer 1
static void* lcd_transfer_buffer1 = NULL;
// transfer buffer 2 (for DMA. See comments in setup())
static void* lcd_transfer_buffer2 = NULL;
// the htcw_uix display object that manages screens:
// you can use a screen directly, and you might
// if your app only has one, but the display object
// allows for easily switching between different screens
static uix::display lcd_display;
// declare our screen type using the display's native pixel format
using screen_t = uix::screen<rgb_pixel<16>>;
// declare the label type with the screens "control surface" type
using label_t = label<typename screen_t::control_surface_type>;
// the color pseudo-enumartion for the screen's native pixel format
using color_t = color<typename screen_t::pixel_type>;


// fire stuff
#define V_WIDTH (240 / 4)
#define V_HEIGHT (136 / 4)
#define BUF_WIDTH (240 / 4)
#define BUF_HEIGHT ((136 / 4) + 6)
#define PALETTE_SIZE (256 * 3)
#define INT_SIZE 2
#ifdef USE_SPANS
#define PAL_TYPE uint16_t
// store preswapped uint16_ts for performance
// these are computed by the compiler, and
// resolve to literal uint16_t values
#define RGB(r,g,b) (rgb_pixel<16>(r,g,b).swapped())
#else
// store rgb_pixel<16> instances
// these are computed by the compiler, and
// resolve to literal uint16_t values
#define PAL_TYPE rgb_pixel<16>
#define RGB(r,g,b) rgb_pixel<16>(r,g,b)
#endif

// color palette for flames
static PAL_TYPE fire_palette[] = {
    RGB(0, 0, 0),    RGB(0, 0, 3),    RGB(0, 0, 3),
    RGB(0, 0, 3),    RGB(0, 0, 4),    RGB(0, 0, 4),
    RGB(0, 0, 4),    RGB(0, 0, 5),    RGB(1, 0, 5),
    RGB(2, 0, 4),    RGB(3, 0, 4),    RGB(4, 0, 4),
    RGB(5, 0, 3),    RGB(6, 0, 3),    RGB(7, 0, 3),
    RGB(8, 0, 2),    RGB(9, 0, 2),    RGB(10, 0, 2),
    RGB(11, 0, 2),   RGB(12, 0, 1),   RGB(13, 0, 1),
    RGB(14, 0, 1),   RGB(15, 0, 0),   RGB(16, 0, 0),
    RGB(16, 0, 0),   RGB(16, 0, 0),   RGB(17, 0, 0),
    RGB(17, 0, 0),   RGB(18, 0, 0),   RGB(18, 0, 0),
    RGB(18, 0, 0),   RGB(19, 0, 0),   RGB(19, 0, 0),
    RGB(20, 0, 0),   RGB(20, 0, 0),   RGB(20, 0, 0),
    RGB(21, 0, 0),   RGB(21, 0, 0),   RGB(22, 0, 0),
    RGB(22, 0, 0),   RGB(23, 1, 0),   RGB(23, 1, 0),
    RGB(24, 2, 0),   RGB(24, 2, 0),   RGB(25, 3, 0),
    RGB(25, 3, 0),   RGB(26, 4, 0),   RGB(26, 4, 0),
    RGB(27, 5, 0),   RGB(27, 5, 0),   RGB(28, 6, 0),
    RGB(28, 6, 0),   RGB(29, 7, 0),   RGB(29, 7, 0),
    RGB(30, 8, 0),   RGB(30, 8, 0),   RGB(31, 9, 0),
    RGB(31, 9, 0),   RGB(31, 10, 0),  RGB(31, 10, 0),
    RGB(31, 11, 0),  RGB(31, 11, 0),  RGB(31, 12, 0),
    RGB(31, 12, 0),  RGB(31, 13, 0),  RGB(31, 13, 0),
    RGB(31, 14, 0),  RGB(31, 14, 0),  RGB(31, 15, 0),
    RGB(31, 15, 0),  RGB(31, 16, 0),  RGB(31, 16, 0),
    RGB(31, 17, 0),  RGB(31, 17, 0),  RGB(31, 18, 0),
    RGB(31, 18, 0),  RGB(31, 19, 0),  RGB(31, 19, 0),
    RGB(31, 20, 0),  RGB(31, 20, 0),  RGB(31, 21, 0),
    RGB(31, 21, 0),  RGB(31, 22, 0),  RGB(31, 22, 0),
    RGB(31, 23, 0),  RGB(31, 24, 0),  RGB(31, 24, 0),
    RGB(31, 25, 0),  RGB(31, 25, 0),  RGB(31, 26, 0),
    RGB(31, 26, 0),  RGB(31, 27, 0),  RGB(31, 27, 0),
    RGB(31, 28, 0),  RGB(31, 28, 0),  RGB(31, 29, 0),
    RGB(31, 29, 0),  RGB(31, 30, 0),  RGB(31, 30, 0),
    RGB(31, 31, 0),  RGB(31, 31, 0),  RGB(31, 32, 0),
    RGB(31, 32, 0),  RGB(31, 33, 0),  RGB(31, 33, 0),
    RGB(31, 34, 0),  RGB(31, 34, 0),  RGB(31, 35, 0),
    RGB(31, 35, 0),  RGB(31, 36, 0),  RGB(31, 36, 0),
    RGB(31, 37, 0),  RGB(31, 38, 0),  RGB(31, 38, 0),
    RGB(31, 39, 0),  RGB(31, 39, 0),  RGB(31, 40, 0),
    RGB(31, 40, 0),  RGB(31, 41, 0),  RGB(31, 41, 0),
    RGB(31, 42, 0),  RGB(31, 42, 0),  RGB(31, 43, 0),
    RGB(31, 43, 0),  RGB(31, 44, 0),  RGB(31, 44, 0),
    RGB(31, 45, 0),  RGB(31, 45, 0),  RGB(31, 46, 0),
    RGB(31, 46, 0),  RGB(31, 47, 0),  RGB(31, 47, 0),
    RGB(31, 48, 0),  RGB(31, 48, 0),  RGB(31, 49, 0),
    RGB(31, 49, 0),  RGB(31, 50, 0),  RGB(31, 50, 0),
    RGB(31, 51, 0),  RGB(31, 52, 0),  RGB(31, 52, 0),
    RGB(31, 52, 0),  RGB(31, 52, 0),  RGB(31, 52, 0),
    RGB(31, 53, 0),  RGB(31, 53, 0),  RGB(31, 53, 0),
    RGB(31, 53, 0),  RGB(31, 54, 0),  RGB(31, 54, 0),
    RGB(31, 54, 0),  RGB(31, 54, 0),  RGB(31, 54, 0),
    RGB(31, 55, 0),  RGB(31, 55, 0),  RGB(31, 55, 0),
    RGB(31, 55, 0),  RGB(31, 56, 0),  RGB(31, 56, 0),
    RGB(31, 56, 0),  RGB(31, 56, 0),  RGB(31, 57, 0),
    RGB(31, 57, 0),  RGB(31, 57, 0),  RGB(31, 57, 0),
    RGB(31, 57, 0),  RGB(31, 58, 0),  RGB(31, 58, 0),
    RGB(31, 58, 0),  RGB(31, 58, 0),  RGB(31, 59, 0),
    RGB(31, 59, 0),  RGB(31, 59, 0),  RGB(31, 59, 0),
    RGB(31, 60, 0),  RGB(31, 60, 0),  RGB(31, 60, 0),
    RGB(31, 60, 0),  RGB(31, 60, 0),  RGB(31, 61, 0),
    RGB(31, 61, 0),  RGB(31, 61, 0),  RGB(31, 61, 0),
    RGB(31, 62, 0),  RGB(31, 62, 0),  RGB(31, 62, 0),
    RGB(31, 62, 0),  RGB(31, 63, 0),  RGB(31, 63, 0),
    RGB(31, 63, 1),  RGB(31, 63, 1),  RGB(31, 63, 2),
    RGB(31, 63, 2),  RGB(31, 63, 3),  RGB(31, 63, 3),
    RGB(31, 63, 4),  RGB(31, 63, 4),  RGB(31, 63, 5),
    RGB(31, 63, 5),  RGB(31, 63, 5),  RGB(31, 63, 6),
    RGB(31, 63, 6),  RGB(31, 63, 7),  RGB(31, 63, 7),
    RGB(31, 63, 8),  RGB(31, 63, 8),  RGB(31, 63, 9),
    RGB(31, 63, 9),  RGB(31, 63, 10), RGB(31, 63, 10),
    RGB(31, 63, 10), RGB(31, 63, 11), RGB(31, 63, 11),
    RGB(31, 63, 12), RGB(31, 63, 12), RGB(31, 63, 13),
    RGB(31, 63, 13), RGB(31, 63, 14), RGB(31, 63, 14),
    RGB(31, 63, 15), RGB(31, 63, 15), RGB(31, 63, 15),
    RGB(31, 63, 16), RGB(31, 63, 16), RGB(31, 63, 17),
    RGB(31, 63, 17), RGB(31, 63, 18), RGB(31, 63, 18),
    RGB(31, 63, 19), RGB(31, 63, 19), RGB(31, 63, 20),
    RGB(31, 63, 20), RGB(31, 63, 21), RGB(31, 63, 21),
    RGB(31, 63, 21), RGB(31, 63, 22), RGB(31, 63, 22),
    RGB(31, 63, 23), RGB(31, 63, 23), RGB(31, 63, 24),
    RGB(31, 63, 24), RGB(31, 63, 25), RGB(31, 63, 25),
    RGB(31, 63, 26), RGB(31, 63, 26), RGB(31, 63, 26),
    RGB(31, 63, 27), RGB(31, 63, 27), RGB(31, 63, 28),
    RGB(31, 63, 28), RGB(31, 63, 29), RGB(31, 63, 29),
    RGB(31, 63, 30), RGB(31, 63, 30), RGB(31, 63, 31),
    RGB(31, 63, 31)};

// it's common in htxw_uix to write small controls in order to do demand draws when
// you need something a bit more involved that what the painter<> control allows for.
template <typename ControlSurfaceType>
class fire_box : public control<ControlSurfaceType> {
    using base_type = control<ControlSurfaceType>;
    int draw_state = 0;
    uint8_t p1[BUF_HEIGHT][BUF_WIDTH];  // VGA buffer, quarter resolution w/extra lines
    unsigned int i, j, k, l, delta;     // looping variables, counters, and data
    char ch;
   public:
    // not necessarly strictly required, but may be expected in larger 
    // apps further down the chain, so they are strongly recommended
    // when making generalized controls. Implemented here mainly
    // for example
    using control_surface_type = ControlSurfaceType;
    using pixel_type = typename base_type::pixel_type;
    using palette_type = typename base_type::palette_type;
    // this constructor not strictly necessary but recommended for generalized controls
    fire_box(uix::invalidation_tracker& parent, const palette_type* palette = nullptr)
        : base_type(parent, palette) {
    }
    // required
    fire_box()
        : base_type() {
    }
    // not strictly necessary but again recommended for generalized controls
    fire_box(fire_box&& rhs) {
        do_move_control(rhs);
        draw_state = 0;
    }
    // not strictly necessary but recommended for generalized controls
    fire_box& operator=(fire_box&& rhs) {
        do_move_control(rhs);
        draw_state = 0;
        return *this;
    }
    // not strictly necessary but recommended for generalized controls
    fire_box(const fire_box& rhs) {
        do_copy_control(rhs);
        draw_state = 0;
    }
    // not strictly necessary but recommended for generalized controls
    fire_box& operator=(const fire_box& rhs) {
        do_copy_control(rhs);
        draw_state = 0;
        return *this;
    }
protected:
    // we require this to compute the frame, as this gets called ONCE
    // per screen draw, whereas on_paint gets called potentially several
    // times
    virtual void on_before_paint() override {
        switch (draw_state) {
            case 0: // first draw
                // Initialize the buffer to 0s
                for (i = 0; i < BUF_HEIGHT; i++) {
                    for (j = 0; j < BUF_WIDTH; j++) {
                        p1[i][j] = 0;
                    }
                }
                draw_state = 1;
                // fall through
            case 1: // first draw and subsequent draws
                // Transform current buffer
                for (i = 1; i < BUF_HEIGHT; ++i) {
                    for (j = 0; j < BUF_WIDTH; ++j) {
                        if (j == 0)
                            p1[i - 1][j] = (p1[i][j] +
                                            p1[i - 1][BUF_WIDTH - 1] +
                                            p1[i][j + 1] +
                                            p1[i + 1][j]) >>
                                           2;
                        else if (j == 79)
                            p1[i - 1][j] = (p1[i][j] +
                                            p1[i][j - 1] +
                                            p1[i + 1][0] +
                                            p1[i + 1][j]) >>
                                           2;
                        else
                            p1[i - 1][j] = (p1[i][j] +
                                            p1[i][j - 1] +
                                            p1[i][j + 1] +
                                            p1[i + 1][j]) >>
                                           2;

                        if (p1[i][j] > 11)
                            p1[i][j] = p1[i][j] - 12;
                        else if (p1[i][j] > 3)
                            p1[i][j] = p1[i][j] - 4;
                        else {
                            if (p1[i][j] > 0) p1[i][j]--;
                            if (p1[i][j] > 0) p1[i][j]--;
                            if (p1[i][j] > 0) p1[i][j]--;
                        }
                    }
                }
                delta = 0;
                // make more 
                for (j = 0; j < BUF_WIDTH; j++) {
                    if (rand() % 10 < 5) {
                        delta = (rand() & 1) * 255;
                    }
                    p1[BUF_HEIGHT - 2][j] = delta;
                    p1[BUF_HEIGHT - 1][j] = delta;
                }
        }
    }
    // here we implement the demand draw. The destination is an htcw_gfx draw target. The clip
    // is the current rectangle within the destination that is actually being drawn, as this
    // routine may be called multiple times to render the entire destination. 
    // Note that the destination and clip use local coordinates, not screen coordinates.
    virtual void on_paint(control_surface_type& destination, const srect16& clip) override {
#ifdef USE_SPANS
        static_assert(gfx::helpers::is_same<rgb_pixel<16>,typename screen_t::pixel_type>::value,"USE_SPANS only works with RGB565");
        for (int y = clip.y1; y <= clip.y2; y+=2) {
            // must use rgb_pixel<16>
            // get the spans for the current partial rows (starting at clip.x1)
            // note that we're getting two, because we draw 2x2 squares
            // of all the same color.
            gfx_span row = destination.span(point16(clip.x1,y));
            gfx_span row2 = destination.span(point16(clip.x1,y+1));
            // get the pointers to the partial row data
            uint16_t *prow = (uint16_t*)row.data;
            uint16_t *prow2 = (uint16_t*)row2.data;
            for (int x = clip.x1; x <= clip.x2; x+=2) {
                int i = y >> 2;
                int j = x >> 2;
                PAL_TYPE px = fire_palette[this->p1[i][j]];
                // set the pixels
                *(prow++)=px;
                // if the clip x ends on an odd value, we need to not set the pointer
                // so check here
                if(x-clip.x1+1<row.length) {
                    *(prow++)=px;
                }
                // the clip y ends on an odd value prow2 will be null
                if(prow2!=nullptr) {
                    *(prow2++)=px;
                    // another check for x if clip ends on an odd value
                    if(x-clip.x1+1<row2.length) {
                        *(prow2++)=px;
                    }
                }                
            }
        }
#else 
        for (int y = clip.y1; y <= clip.y2; ++y) {
            for (int x = clip.x1; x <= clip.x2; ++x) {
                int i = y >> 2;
                int j = x >> 2;
                PAL_TYPE px = fire_palette[this->p1[i][j]];
                // set the pixel
                destination.point(point16(x,y),px);
            }
        }
#endif               
    }
    
};
esp_lcd_panel_handle_t lcd_handle = nullptr;
static bool lcd_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lcd_display.flush_complete();
    return true;
}

static void lcd_initialize()
{
    spi_bus_config_t buscfg;
    memset(&buscfg, 0, sizeof(buscfg));
    buscfg.sclk_io_num = 18;
    buscfg.mosi_io_num = 19;
    buscfg.miso_io_num = -1;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = sizeof(lcd_transfer_size) + 8;

    // Initialize the SPI bus on VSPI (SPI3)
    spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config;
    memset(&io_config, 0, sizeof(io_config));
    io_config.dc_gpio_num = 16,
    io_config.cs_gpio_num = 5,
    io_config.pclk_hz = 40 * 1000 * 1000,
    io_config.lcd_cmd_bits = 8,
    io_config.lcd_param_bits = 8,
    io_config.spi_mode = 0,
    io_config.trans_queue_depth = 10,
    io_config.on_color_trans_done = lcd_flush_ready;
    // Attach the LCD to the SPI bus
    esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI3_HOST, &io_config, &io_handle);

    lcd_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config;
    memset(&panel_config, 0, sizeof(panel_config));
    panel_config.reset_gpio_num = 23;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    panel_config.rgb_endian = LCD_RGB_ENDIAN_RGB;
#else
    panel_config.color_space = ESP_LCD_COLOR_SPACE_RGB;
#endif
    panel_config.bits_per_pixel = 16;

    // Initialize the LCD configuration
    esp_lcd_new_panel_st7789(io_handle, &panel_config, &lcd_handle);

    // Turn off backlight to avoid unpredictable display on the LCD screen while initializing
    // the LCD panel driver. (Different LCD screens may need different levels)
    gpio_set_level((gpio_num_t)4, 0);
    // Reset the display
    esp_lcd_panel_reset(lcd_handle);

    // Initialize LCD panel
    esp_lcd_panel_init(lcd_handle);
    // esp_lcd_panel_io_tx_param(io_handle, LCD_CMD_SLPOUT, NULL, 0);
    //  Swap x and y axis (Different LCD screens may need different options)
    esp_lcd_panel_swap_xy(lcd_handle, true);
    esp_lcd_panel_set_gap(lcd_handle, 40, 52);
    esp_lcd_panel_mirror(lcd_handle, false, true);
    esp_lcd_panel_invert_color(lcd_handle, true);
    // Turn on the screen
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_lcd_panel_disp_on_off(lcd_handle, true);
#else
    esp_lcd_panel_disp_off(lcd_handle, false);
#endif
    // Turn on backlight (Different LCD screens may need different levels)
    // gpio_set_level((gpio_num_t)4, 1);
}
// declare the fire box type using the screen type's control surface type
using fire_box_t = fire_box<typename screen_t::control_surface_type>;

// for access to RGB8888 colors which htcw_uix controls use
using color32_t = color<rgba_pixel<32>>;

// callback for writing data to the display
static void uix_on_flush(const rect16& bounds,
                             const void *bitmap, void* state) {
    esp_lcd_panel_draw_bitmap(lcd_handle, bounds.x1, bounds.y1, bounds.x2+1, bounds.y2+1, bitmap);
}
// flush_complete() must be called on the display object any time
// a flush is done, even if it is synchronous (not using DMA).
// Since we're using DMA, we get this lcd_on_flush_complete()
// from the freenove driver, and let htcw_uix know we're done
// flushing
void lcd_on_flush_complete() {
    lcd_display.flush_complete();
}
// fonts are read from streams, so wrap a stream around our
// embedded win 3.1 .fon file byte array
static const_buffer_stream fps_font_stream(vga_8x8,sizeof(vga_8x8));
// construct the font with the stream we just made
static win_font fps_font(fps_font_stream);
// the main screen where our controls reside
static screen_t main_screen;
// the FPS label which displays statistics
static label_t fps_label;
// the fire box control from above
static fire_box_t main_fire;
void setup() {
    Serial.begin(115200);
    // we use two transfer buffers so htcw_uix can draw to one while sending
    // the other at the same time to maximize throughput. Here we allocate both
    lcd_transfer_buffer1 = heap_caps_malloc(lcd_transfer_size, MALLOC_CAP_DMA);
    lcd_transfer_buffer2 = heap_caps_malloc(lcd_transfer_size, MALLOC_CAP_DMA);
    if (lcd_transfer_buffer1 == NULL || lcd_transfer_buffer2 == NULL) {
        puts("Could not allocate transfer buffers");
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }
    // initialize the lcd
    lcd_initialize();
    button_a.initialize();
    button_b.initialize();
    lcd_light.initialize();
    
    lcd_display.buffer_size(lcd_transfer_size);
    // set the buffers. if we only used one,
    // we would not take advantage of ESP32 DMA
    lcd_display.buffer1((uint8_t*)lcd_transfer_buffer1);
    lcd_display.buffer2((uint8_t*)lcd_transfer_buffer2);
    // give the display object our flush callback
    // so it can send bitmaps to the display
    lcd_display.on_flush_callback(uix_on_flush);
    // set the screen dimensions (important!)
    main_screen.dimensions({240,135});
    // defaults to black, but we set it anyway
    // for example. Note that this is in the 
    // *screen*'s color format, not htcw_uix's
    // native color format, which is 32-bit RGBA
    main_screen.background_color(color_t::black);
    // set the bounds for our custom fire control
    // it takes up the whole screen
    main_fire.bounds(main_screen.bounds());
    // register the control with the screen
    main_screen.register_control(main_fire);
    // initialize the font
    fps_font.initialize();
    // set the label color
    fps_label.color(color32_t::blue);
    // set the bounds for the label (near the bottom)
    fps_label.bounds({0,125,239,125+8});
    // want to align the text to the right
    fps_label.text_justify(uix_justify::top_right);
    // set the font to use (from above)
    fps_label.font(fps_font);
    // we don't want any padding around the text
    fps_label.padding({0,0});
    // for now set the text to nothing - to be
    // set later after we compute the FPS
    fps_label.text("");
    // register the label with the screem
    main_screen.register_control(fps_label);
    // tell the display object to use the screen
    lcd_display.active_screen(main_screen);
    // print the free ram
    printf("Free SRAM: %0.2fKB, free PSRAM: %0.2fMB\n",
           heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024.f,
           heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024.f / 1024.f);
}
// information for our FPS statistics:
static int frames = 0;
static uint32_t total_ms = 0;
static uint32_t ts_ms = 0;
static char fps_buf[64];
void loop() {
    uint32_t start_ms = pdTICKS_TO_MS(xTaskGetTickCount());
    // force the fire control to repaint
    main_fire.invalidate();
    // update the display - causes the frame to be 
    // flushed to the display.
    lcd_display.update();
    lcd_light.update();
    button_a.update();
    button_b.update();
    // wake backlight on button press
    if(button_a.pressed() || button_b.pressed()) {
        lcd_light.wake();
    }
    // when we have a flush pending, that means htcw_uix returned
    // immediately due to the DMA being already tied up with a previous
    // transaction. It will continue rendering once it is complete.
    // However, we don't want to count a pending as one of the frames
    // for calculating the FPS, since rendering was effectively skipped,
    // or rather, delayed - no paint actually happened, yet.
    if(!lcd_display.flush_pending()) {
        // compute the statistics
        ++frames;
        uint32_t end_ms = pdTICKS_TO_MS(xTaskGetTickCount());

        total_ms += (end_ms - start_ms);
        if (end_ms > ts_ms + 1000) {
            ts_ms = end_ms;
            // make sure we don't div by zero
            if (frames > 0) {
                sprintf(fps_buf, "FPS: %d, avg ms: %0.2f", frames,
                    (float)total_ms / (float)frames);
            } else {
                sprintf(fps_buf, "FPS: < 1, total ms: %d", (int)total_ms);
            }
            // update the label (redraw is "automatic" (happens during lcd_display.update()))
            fps_label.text(fps_buf);
            puts(fps_buf);
            total_ms = 0;
            frames = 0;
        }
    } 
}
