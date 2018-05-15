// Display Library for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
// Requires HW SPI and Adafruit_GFX. Caution: these e-papers require 3.3V supply AND data lines!
//
// based on Demo Example from Good Display: http://www.good-display.com/download_list/downloadcategoryid=34&isMode=false.html
//
// Author: Jean-Marc Zingg
//
// Version: see library.properties
//
// Library: https://github.com/ZinggJM/GxEPD2

#ifndef _GxEPD2_3C_H_
#define _GxEPD2_3C_H_

#include <Adafruit_GFX.h>
#include "GxEPD2_EPD.h"
#include "epd3c/GxEPD2_154c.h"
#include "epd3c/GxEPD2_213c.h"
#include "epd3c/GxEPD2_290c.h"
#include "epd3c/GxEPD2_270c.h"
#include "epd3c/GxEPD2_420c.h"
#include "epd3c/GxEPD2_583c.h"
#include "epd3c/GxEPD2_750c.h"

template<typename GxEPD2_Type, const uint16_t page_height>
class GxEPD2_3C : public Adafruit_GFX
{
  public:
    GxEPD2_Type epd2;
    GxEPD2_3C(GxEPD2_Type epd2_instance) : Adafruit_GFX(GxEPD2_Type::WIDTH, GxEPD2_Type::HEIGHT), epd2(epd2_instance)
    {
      _page_height = page_height;
      _pages = (HEIGHT / _page_height) + ((HEIGHT % _page_height) > 0);
      _reverse = (epd2_instance.panel == GxEPD2::GDE0213B1);
      _using_partial_mode = false;
      _current_page = 0;
      setFullWindow();
    }

    bool mirror(bool m)
    {
      swap (_mirror, m);
      return m;
    }

    void drawPixel(int16_t x, int16_t y, uint16_t color)
    {
      if ((x < 0) || (x >= width()) || (y < 0) || (y >= height())) return;
      if (_mirror) x = width() - x - 1;
      // check rotation, move pixel around if necessary
      switch (getRotation())
      {
        case 1:
          swap(x, y);
          x = WIDTH - x - 1;
          break;
        case 2:
          x = WIDTH - x - 1;
          y = HEIGHT - y - 1;
          break;
        case 3:
          swap(x, y);
          y = HEIGHT - y - 1;
          break;
      }
      // adjust for current page
      y -= _current_page * _page_height;
      // adjust for partial window
      x -= _pw_x;
      y -= _pw_y % _page_height;
      if (_reverse) y = _pw_h - y - 1;
      // check if in current page
      if (y >= _page_height) return;
      // check if in (partial) window
      if ((x < 0) || (x >= _pw_w) || (y < 0) || (y >= _pw_h)) return;
      uint16_t i = x / 8 + y * (_pw_w / 8);
      _black_buffer[i] = (_black_buffer[i] | (1 << (7 - x % 8))); // white
      _color_buffer[i] = _color_buffer[i] = (_color_buffer[i] | (1 << (7 - x % 8)));
      if (color == GxEPD_WHITE) return;
      else if (color == GxEPD_BLACK) _black_buffer[i] = (_black_buffer[i] & (0xFF ^ (1 << (7 - x % 8))));
      else if (color == GxEPD_RED) _color_buffer[i] = (_color_buffer[i] & (0xFF ^ (1 << (7 - x % 8))));
    }

    void init(uint32_t serial_diag_bitrate = 0) // = 0 : disabled
    {
      epd2.init(serial_diag_bitrate);
      _using_partial_mode = false;
      _current_page = 0;
      setFullWindow();
    }

    void fillScreen(uint16_t color) // 0x0 black, >0x0 white, to buffer
    {
      uint8_t black = 0xFF;
      uint8_t red = 0xFF;
      if (color == GxEPD_WHITE);
      else if (color == GxEPD_BLACK) black = 0x00;
      else if (color == GxEPD_RED) red = 0x00;
      for (uint16_t x = 0; x < sizeof(_black_buffer); x++)
      {
        _black_buffer[x] = black;
        _color_buffer[x] = red;
      }
    }

    // display buffer content to screen, useful for full screen buffer
    void display(bool partial_update_mode = false)
    {
      epd2.writeImage(_black_buffer, _color_buffer, 0, 0, WIDTH, HEIGHT);
      epd2.refresh(partial_update_mode);
    }

    void setFullWindow()
    {
      _using_partial_mode = false;
      _pw_x = 0;
      _pw_y = 0;
      _pw_w = WIDTH;
      _pw_h = HEIGHT;
    }

    void setPartialWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
    {
      if (!epd2.hasPartialUpdate) return;
      _rotate(x, y, w, h);
      _using_partial_mode = true;
      _pw_x = gx_uint16_min(x, WIDTH);
      _pw_y = gx_uint16_min(y, HEIGHT);
      _pw_w = gx_uint16_min(w, WIDTH - _pw_x);
      _pw_h = gx_uint16_min(h, HEIGHT - _pw_y);
      // make _pw_x, _pw_w multiple of 8
      _pw_w += _pw_x % 8;
      if (_pw_w % 8 > 0) _pw_w += 8 - _pw_w % 8;
      _pw_x -= _pw_x % 8;
    }

    void firstPage()
    {
      fillScreen(GxEPD_WHITE);
      _current_page = 0;
      _second_phase = false;
    }

    bool nextPage()
    {
      uint16_t page_ys = _current_page * _page_height;
      if (_using_partial_mode)
      {
        //Serial.print("  nextPage("); Serial.print(_pw_x); Serial.print(", "); Serial.print(_pw_y); Serial.print(", ");
        //Serial.print(_pw_w); Serial.print(", "); Serial.print(_pw_h); Serial.print(") P"); Serial.println(_current_page);
        uint16_t page_ye = _current_page < (_pages - 1) ? page_ys + _page_height : HEIGHT;
        uint16_t dest_ys = gx_uint16_max(_pw_y, page_ys);
        uint16_t dest_ye = gx_uint16_min(_pw_y + _pw_h, page_ye);
        if (dest_ye > dest_ys)
        {
          //Serial.print("writeImage("); Serial.print(_pw_x); Serial.print(", "); Serial.print(dest_ys); Serial.print(", ");
          //Serial.print(_pw_w); Serial.print(", "); Serial.print(dest_ye - dest_ys); Serial.println(")");
          epd2.writeImage(_black_buffer, _color_buffer, _pw_x, dest_ys, _pw_w, dest_ye - dest_ys);
        }
        else
        {
          //Serial.print("writeImage("); Serial.print(_pw_x); Serial.print(", "); Serial.print(dest_ys); Serial.print(", ");
          //Serial.print(_pw_w); Serial.print(", "); Serial.print(dest_ye - dest_ys); Serial.print(") skipped ");
          //Serial.print(dest_ys); Serial.print(".."); Serial.println(dest_ye);
        }
        _current_page++;
        if (_current_page == _pages)
        {
          _current_page = 0;
          if (!_second_phase)
          {
            epd2.refresh(_pw_x, _pw_y, _pw_w, _pw_h);
            if (epd2.hasFastPartialUpdate)
            {
              _second_phase = true;
              return true;
            }
          }
          return false;
        }
        fillScreen(GxEPD_WHITE);
        return true;
      }
      else
      {
        epd2.writeImage(_black_buffer, _color_buffer, 0, page_ys, WIDTH, gx_uint16_min(_page_height, HEIGHT - page_ys));
        _current_page++;
        if (_current_page == _pages)
        {
          _current_page = 0;
          if (epd2.hasFastPartialUpdate)
          {
            if (!_second_phase)
            {
              epd2.refresh(false);
              _second_phase = true;
              fillScreen(GxEPD_WHITE);
              return true;
            }
            else epd2.refresh(true);
          } else epd2.refresh(false);
          epd2.powerOff();
          return false;
        }
        fillScreen(GxEPD_WHITE);
        return true;
      }
    }

    // GxEPD style paged drawing; drawCallback() is called as many times as needed
    void drawPaged(void (*drawCallback)(const void*), const void* pv)
    {
      if (_using_partial_mode)
      {
        for (_current_page = 0; _current_page < _pages; _current_page++)
        {
          uint16_t page_ys = _current_page * _page_height;
          uint16_t page_ye = _current_page < (_pages - 1) ? page_ys + _page_height : HEIGHT;
          uint16_t dest_ys = gx_uint16_max(_pw_y, page_ys);
          uint16_t dest_ye = gx_uint16_min(_pw_y + _pw_h, page_ye);
          if (dest_ye > dest_ys)
          {
            fillScreen(GxEPD_WHITE);
            drawCallback(pv);
            epd2.writeImage(_black_buffer, _color_buffer, _pw_x, dest_ys, _pw_w, dest_ye - dest_ys);
          }
        }
        epd2.refresh(_pw_x, _pw_y, _pw_w, _pw_h);
      }
      else
      {
        for (_current_page = 0; _current_page < _pages; _current_page++)
        {
          uint16_t page_ys = _current_page * _page_height;
          fillScreen(GxEPD_WHITE);
          drawCallback(pv);
          epd2.writeImage(_black_buffer, _color_buffer, 0, page_ys, WIDTH, gx_uint16_min(_page_height, HEIGHT - page_ys));
        }
        epd2.refresh();
      }
      _current_page = 0;
    }

    void drawInvertedBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color)
    {
      // taken from Adafruit_GFX.cpp, modified
      int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
      uint8_t byte = 0;
      for (int16_t j = 0; j < h; j++)
      {
        for (int16_t i = 0; i < w; i++ )
        {
          if (i & 7) byte <<= 1;
          else
          {
#if defined(__AVR) || defined(ESP8266) || defined(ESP32)
            byte = pgm_read_byte(&bitmap[j * byteWidth + i / 8]);
#else
            byte = bitmap[j * byteWidth + i / 8];
#endif
          }
          if (!(byte & 0x80))
          {
            drawPixel(x + i, y + j, color);
          }
        }
      }
    }

    //  Support for Bitmaps (Sprites) to Controller Buffer and to Screen
    void clearScreen(uint8_t value = 0xFF) // init controller memory and screen (default white)
    {
      epd2.clearScreen(value);
    }
    void writeScreenBuffer(uint8_t value = 0xFF) // init controller memory (default white)
    {
      epd2.writeScreenBuffer(value);
    }
    // write to controller memory, without screen refresh; x and w should be multiple of 8
    void writeImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false)
    {
      epd2.writeImage(bitmap, x, y, w, h, invert, mirror_y, pgm);
    }
    void writeImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false)
    {
      epd2.writeImage(black, color, x, y, w, h, invert, mirror_y, pgm);
    }
    // write sprite of native data to controller memory, without screen refresh; x and w should be multiple of 8
    void writeNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
    {
      epd2.writeNative(data1, data2, x, y, w, h, invert, mirror_y, pgm);
    }
    // write to controller memory, with screen refresh; x and w should be multiple of 8
    void drawImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false)
    {
      epd2.drawImage(bitmap, x, y, w, h, invert, mirror_y, pgm);
    }
    void drawImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false)
    {
      epd2.drawImage(black, color, x, y, w, h, invert, mirror_y, pgm);
    }
    // write sprite of native data to controller memory, with screen refresh; x and w should be multiple of 8
    void drawNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
    {
      epd2.drawNative(data1, data2, x, y, w, h, invert, mirror_y, pgm);
    }
    void refresh(bool partial_update_mode = false) // screen refresh from controller memory to full screen
    {
      epd2.refresh(partial_update_mode);
    }
    void refresh(int16_t x, int16_t y, int16_t w, int16_t h) // screen refresh from controller memory, partial screen
    {
      epd2.refresh(x, y, w, h);
    }
    void powerOff()
    {
      epd2.powerOff();
    }
  private:
    template <typename T> static inline void
    swap(T & a, T & b)
    {
      T t = a;
      a = b;
      b = t;
    };
    static inline uint16_t gx_uint16_min(uint16_t a, uint16_t b)
    {
      return (a < b ? a : b);
    };
    static inline uint16_t gx_uint16_max(uint16_t a, uint16_t b)
    {
      return (a > b ? a : b);
    };
    void _rotate(uint16_t& x, uint16_t& y, uint16_t& w, uint16_t& h)
    {
      switch (getRotation())
      {
        case 1:
          swap(x, y);
          swap(w, h);
          x = WIDTH - x - w;
          break;
        case 2:
          x = WIDTH - x - w;
          y = HEIGHT - y - h;
          break;
        case 3:
          swap(x, y);
          swap(w, h);
          y = HEIGHT - y - h;
          break;
      }
    }
  private:
    uint8_t _black_buffer[(GxEPD2_Type::WIDTH / 8) * page_height];
    uint8_t _color_buffer[(GxEPD2_Type::WIDTH / 8) * page_height];
    bool _using_partial_mode, _second_phase, _mirror, _reverse;
    uint16_t _width_bytes, _pixel_bytes;
    int16_t _current_page;
    uint16_t _pages, _page_height;
    uint16_t _pw_x, _pw_y, _pw_w, _pw_h;
};

#endif
