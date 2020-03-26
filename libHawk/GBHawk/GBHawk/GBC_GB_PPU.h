#include <iostream>
#include <cstdint>
#include <iomanip>
#include <string>
#include <math.h>

#include "PPU_Base.h"

using namespace std;

namespace GBHawk
{
	class GBC_GB_PPU : public PPU
	{
	public:
		uint8_t ReadReg(uint32_t addr)
		{
			uint8_t ret = 0;
			//Console.WriteLine(Core.cpu.TotalExecutedCycles);
			switch (addr)
			{
			case 0xFF40: ret = LCDC;							break; // LCDC
			case 0xFF41: ret = STAT;							break; // STAT
			case 0xFF42: ret = scroll_y;						break; // SCY
			case 0xFF43: ret = scroll_x;						break; // SCX
			case 0xFF44: ret = LY;								break; // LY
			case 0xFF45: ret = LYC;								break; // LYC
			case 0xFF46: ret = DMA_addr;						break; // DMA 
			case 0xFF47: ret = BGP;								break; // BGP
			case 0xFF48: ret = obj_pal_0;						break; // OBP0
			case 0xFF49: ret = obj_pal_1;						break; // OBP1
			case 0xFF4A: ret = window_y;						break; // WY
			case 0xFF4B: ret = window_x;						break; // WX

			// These are GBC specific Regs
			case 0xFF51: ret = HDMA_src_hi;						break; // HDMA1
			case 0xFF52: ret = HDMA_src_lo;						break; // HDMA2
			case 0xFF53: ret = HDMA_dest_hi;					break; // HDMA3
			case 0xFF54: ret = HDMA_dest_lo;					break; // HDMA4
			case 0xFF55: ret = HDMA_ctrl();						break; // HDMA5
			case 0xFF68: ret = BG_pal_ret();					break; // BGPI
			case 0xFF69: ret = BG_PAL_read();					break; // BGPD
			case 0xFF6A: ret = OBJ_pal_ret();					break; // OBPI
			case 0xFF6B: ret = OBJ_bytes[OBJ_bytes_index];		break; // OBPD
			}

			return ret;
		}

		uint8_t BG_PAL_read()
		{
			if (VRAM_access_read)
			{
				return BG_bytes[BG_bytes_index];
			}
			else
			{
				return 0xFF;
			}
		}

		void WriteReg(uint32_t addr, uint8_t value)
		{
			switch (addr)
			{
			case 0xFF40: // LCDC
				if (((LCDC & 0x80) > 0) && !((value & 0x80) > 0))
				{
					VRAM_access_read = true;
					VRAM_access_write = true;
					OAM_access_read = true;
					OAM_access_write = true;
				}

				if (!((LCDC & 0x80) > 0) && ((value & 0x80) > 0))
				{
					// don't draw for one frame after turning on
					blank_frame = true;
				}

				LCDC = value;
				break;
			case 0xFF41: // STAT
				// note that their is no stat interrupt bug in GBC
				STAT = (uint8_t)((value & 0xF8) | (STAT & 7) | 0x80);

				if (((STAT & 3) == 0) && ((STAT & 0x8) > 0)) { HBL_INT = true; }
				else { HBL_INT = false; }
				if (((STAT & 3) == 1) && ((STAT & 0x10) > 0)) { VBL_INT = true; }
				else { VBL_INT = false; }
				// OAM not triggered?
				// if (((STAT & 3) == 2) && STAT.Bit(5)) { OAM_INT = true; } else { OAM_INT = false; }

				if (((value & 0x40) > 0) && ((LCDC & 0x80) > 0))
				{
					if (LY == LYC) { LYC_INT = true; }
					else { LYC_INT = false; }
				}
				if (!((STAT & 0x40) > 0)) { LYC_INT = false; }
				break;
			case 0xFF42: // SCY
				scroll_y = value;
				break;
			case 0xFF43: // SCX
				scroll_x = value;
				break;
			case 0xFF44: // LY
				LY = 0; /*reset*/
				break;
			case 0xFF45:  // LYC
				// tests indicate that latching writes to LYC should take place 4 cycles after the write
				// otherwise tests around LY boundaries will fail
				LYC_t = value;
				LYC_cd = 4;
				break;
			case 0xFF46: // DMA 
				DMA_addr = value;
				DMA_start = true;
				DMA_OAM_access = true;
				DMA_clock = 0;
				DMA_inc = 0;
				break;
			case 0xFF47: // BGP
				BGP = value;
				break;
			case 0xFF48: // OBP0
				obj_pal_0 = value;
				break;
			case 0xFF49: // OBP1
				obj_pal_1 = value;
				break;
			case 0xFF4A: // WY
				window_y = value;
				break;
			case 0xFF4B: // WX
				window_x = value;
				break;

				// These are GBC specific Regs
			case 0xFF51: // HDMA1
				HDMA_src_hi = value;
				cur_DMA_src = (uint32_t)(((HDMA_src_hi & 0xFF) << 8) | (cur_DMA_src & 0xF0));
				break;
			case 0xFF52: // HDMA2
				HDMA_src_lo = value;
				cur_DMA_src = (uint32_t)((cur_DMA_src & 0xFF00) | (HDMA_src_lo & 0xF0));
				break;
			case 0xFF53: // HDMA3
				HDMA_dest_hi = value;
				cur_DMA_dest = (uint32_t)(((HDMA_dest_hi & 0x1F) << 8) | (cur_DMA_dest & 0xF0));
				break;
			case 0xFF54: // HDMA4
				HDMA_dest_lo = value;
				cur_DMA_dest = (uint32_t)((cur_DMA_dest & 0xFF00) | (HDMA_dest_lo & 0xF0));
				break;
			case 0xFF55: // HDMA5
				if (!HDMA_active)
				{
					HDMA_mode = ((value & 0x80) > 0);
					HDMA_countdown = 4;
					HDMA_tick = 0;
					if (((value & 0x80) > 0))
					{
						// HDMA during HBlank only, but only if screen is on, otherwise DMA immediately one block of data
						// worms armaggedon requires HDMA to fire in hblank mode even if the screen is off.
						HDMA_active = true;
						HBL_HDMA_count = 0x10;

						last_HBL = LY - 1;

						HBL_test = true;
						HBL_HDMA_go = false;

						if (!((LCDC & 0x80) > 0))
						{
							HDMA_run_once = true;
						}
					}
					else
					{
						// HDMA immediately
						HDMA_active = true;
						HDMA_transfer[0] = true;
					}

					HDMA_length = ((value & 0x7F) + 1) * 16;
				}
				else
				{
					//terminate the transfer
					if (!((value & 0x80) > 0))
					{
						HDMA_active = false;
					}
				}

				break;
			case 0xFF68: // BGPI
				BG_bytes_index = (uint8_t)(value & 0x3F);
				BG_bytes_inc = ((value & 0x80) == 0x80);
				break;
			case 0xFF69: // BGPD
				if (VRAM_access_write)
				{
					BG_transfer_byte = value;
					BG_bytes[BG_bytes_index] = value;
				}

				// change the appropriate palette color
				color_compute_BG();
				if (BG_bytes_inc) { BG_bytes_index++; BG_bytes_index &= 0x3F; }
				break;
			case 0xFF6A: // OBPI
				OBJ_bytes_index = (uint8_t)(value & 0x3F);
				OBJ_bytes_inc = ((value & 0x80) == 0x80);
				break;
			case 0xFF6B: // OBPD
				OBJ_transfer_byte = value;
				OBJ_bytes[OBJ_bytes_index] = value;

				// change the appropriate palette color
				color_compute_OBJ();

				if (OBJ_bytes_inc) { OBJ_bytes_index++; OBJ_bytes_index &= 0x3F; }
				break;
			}
		}

		void tick()
		{
			// Do HDMA ticks
			if (HDMA_active)
			{
				if (HDMA_length > 0)
				{
					if (!HDMA_mode)
					{
						if (HDMA_countdown > 0)
						{
							HDMA_countdown--;
						}
						else
						{
							// immediately transfer bytes, 2 bytes per cycles
							if ((HDMA_tick % 2) == 0)
							{
								HDMA_byte = ReadMemory(cur_DMA_src);
							}
							else
							{
								VRAM[(VRAM_Bank[0] * 0x2000) + cur_DMA_dest] = HDMA_byte;
								cur_DMA_dest = (uint8_t)((cur_DMA_dest + 1) & 0x1FFF);
								cur_DMA_src = (uint8_t)((cur_DMA_src + 1) & 0xFFFF);
								HDMA_length--;
							}

							HDMA_tick++;
						}
					}
					else
					{
						// only transfer during mode 0, and only 16 bytes at a time
						if (((STAT & 3) == 0) && (LY != last_HBL) && HBL_test && (LY_inc == 1) && (cycle > 4))
						{
							HBL_HDMA_go = true;
							HBL_test = false;
						}
						else if (HDMA_run_once)
						{
							HBL_HDMA_go = true;
							HBL_test = false;
							HDMA_run_once = false;
						}

						if (HBL_HDMA_go && (HBL_HDMA_count > 0))
						{
							HDMA_transfer[0] = true;

							if (HDMA_countdown > 0)
							{
								HDMA_countdown--;
							}
							else
							{
								if ((HDMA_tick % 2) == 0)
								{
									HDMA_byte = ReadMemory(cur_DMA_src);
								}
								else
								{
									VRAM[(VRAM_Bank[0] * 0x2000) + cur_DMA_dest] = HDMA_byte;
									cur_DMA_dest = (uint32_t)((cur_DMA_dest + 1) & 0x1FFF);
									cur_DMA_src = (uint32_t)((cur_DMA_src + 1) & 0xFFFF);
									HDMA_length--;
									HBL_HDMA_count--;
								}

								if ((HBL_HDMA_count == 0) && (HDMA_length != 0))
								{

									HBL_test = true;
									last_HBL = LY;
									HBL_HDMA_count = 0x10;
									HBL_HDMA_go = false;
									HDMA_countdown = 4;
								}

								HDMA_tick++;
							}
						}
						else
						{
							HDMA_transfer = false;
						}
					}
				}
				else
				{
					HDMA_active = false;
					HDMA_transfer = false;
				}
			}

			// the ppu only does anything if it is turned on via bit 7 of LCDC
			if (((LCDC & 0x80) > 0))
			{
				// start the next scanline
				if (cycle == 456)
				{
					// scanline callback
					if ((LY + LY_inc) == _scanlineCallbackLine[0])
					{
						//if (Core._scanlineCallback != null)
						//{
						//	Core.GetGPU();
						//	Core._scanlineCallback(LCDC);
						//}
					}

					cycle = 0;
					LY += LY_inc;
					cpu_LY[0] = LY;

					no_scan = false;

					if (LY == 0 && LY_inc == 0)
					{
						LY_inc = 1;
						in_vblank[0] = false;

						//STAT &= 0xFC;

						// special note here, the y coordiate of the window is kept if the window is deactivated
						// meaning it will pick up where it left off if re-enabled later
						// so we don't reset it in the scanline loop
						window_y_tile = 0;
						window_y_latch = window_y;
						window_y_tile_inc = 0;
						window_started = false;
						if (!((LCDC & 0x20) > 0)) { window_is_reset = true; }
					}

					// Automatically restore access to VRAM at this time (force end drawing)
					// Who Framed Roger Rabbit seems to run into this.
					VRAM_access_write = true;
					VRAM_access_read = true;

					if (LY == 144)
					{
						in_vblank[0] = true;
					}
				}

				// exit vblank if LCD went from off to on
				if (LCD_was_off)
				{
					//VBL_INT = false;
					in_vblank[0] = false;
					LCD_was_off = false;

					// we exit vblank into mode 0 for 4 cycles 
					// but no hblank interrupt, presumably this only happens transitioning from mode 3 to 0
					STAT &= 0xFC;

					// also the LCD doesn't turn on right away
					// also, the LCD does not enter mode 2 on scanline 0 when first turned on
					no_scan = true;
					cycle = 8;
				}

				// the VBL stat is continuously asserted
				if (LY >= 144)
				{
					if (((STAT & 0x10) > 0))
					{
						if ((cycle >= 4) && (LY == 144))
						{
							VBL_INT = true;
						}
						else if (LY > 144)
						{
							VBL_INT = true;
						}
					}

					if ((cycle == 2) && (LY == 144))
					{
						// there is an edge case where a VBL INT is triggered if STAT bit 5 is set
						if (((STAT & 0x20) > 0)) { VBL_INT = true; }
					}

					if ((cycle == 4) && (LY == 144))
					{
						HBL_INT = false;

						// set STAT mode to 1 (VBlank) and interrupt flag if it is enabled
						STAT &= 0xFC;
						STAT |= 0x01;

						if ((REG_FFFF[0] & 1) > 0) { FlagI[0] = true; }
						REG_FF0F[0] |= 0x01;
					}

					if ((cycle == 4) && (LY == 144))
					{
						if (((STAT & 0x20) > 0)) { VBL_INT = false; }
					}

					if ((cycle == 8) && (LY == 153))
					{
						LY = 0;
						LY_inc = 0;
						cpu_LY[0] = LY;
					}
				}

				if (!in_vblank[0])
				{
					if (no_scan)
					{
						// timings are slightly different if we just turned on the LCD
						// there is no mode 2  (presumably it missed the trigger)
						if (cycle < 85)
						{
							if (cycle == 8)
							{
								// clear the sprite table
								for (uint32_t k = 0; k < 10; k++)
								{
									SL_sprites[k * 4] = 0;
									SL_sprites[k * 4 + 1] = 0;
									SL_sprites[k * 4 + 2] = 0;
									SL_sprites[k * 4 + 3] = 0;
								}

								if (LY != LYC)
								{
									LYC_INT = false;
									STAT &= 0xFB;
								}

								if ((LY == LYC) && !((STAT & 0x4) > 0))
								{
									// set STAT coincidence FLAG and interrupt flag if it is enabled
									STAT |= 0x04;
									if (((STAT & 0x40) > 0)) { LYC_INT = true; }
								}
							}

							if (cycle == 84)
							{
								STAT &= 0xFC;
								STAT |= 0x03;
								OAM_INT = false;

								OAM_access_read = false;
								OAM_access_write = false;
								VRAM_access_read = false;
								VRAM_access_write = false;
							}
						}
						else
						{
							if (cycle >= 85)
							{
								if (cycle == 85)
								{
									// x-scroll is expected to be latched one cycle later 
									// this is fine since nothing has started in the rendering until the second cycle
									// calculate the column number of the tile to start with
									x_tile = (uint32_t)floor((float)(scroll_x) / 8.0);
									render_offset = scroll_x % 8;
								}

								// render the screen and handle hblank
								render(cycle - 85);
							}
						}
					}
					else
					{
						if (cycle <= 80)
						{
							if (cycle == 2)
							{
								if (LY != 0)
								{
									HBL_INT = false;

									if (((STAT & 0x20) > 0)) { OAM_INT = true; }
								}
							}
							else if (cycle == 4)
							{
								// here mode 2 will be set to true and interrupts fired if enabled
								STAT &= 0xFC;
								STAT |= 0x2;

								if (LY == 0)
								{
									VBL_INT = false;
									if (((STAT & 0x20) > 0)) { OAM_INT = true; }
								}
							}

							if (cycle == 80)
							{
								OAM_access_read = false;
								OAM_access_write = true;
								VRAM_access_read = false;
							}
							else
							{
								// here OAM scanning is performed
								OAM_scan(cycle);
							}
						}
						else if (cycle >= 83)
						{
							if (cycle == 84)
							{
								STAT &= 0xFC;
								STAT |= 0x03;
								OAM_INT = false;
								OAM_access_write = false;
								VRAM_access_write = false;

								// x-scroll is expected to be latched one cycle later 
								// this is fine since nothing has started in the rendering until the second cycle
								// calculate the column number of the tile to start with
								x_tile = (uint32_t)floor((float)(scroll_x) / 8.0);
								render_offset = scroll_x % 8;
							}

							// render the screen and handle hblank
							render(cycle - 83);
						}
					}
				}

				if (LY_inc == 0)
				{
					if (cycle == 12)
					{
						LYC_INT = false;
						STAT &= 0xFB;
					}
					else if (cycle == 14)
					{
						// Special case of LY = LYC
						if ((LY == LYC) && !((STAT & 0x4) > 0))
						{
							// set STAT coincidence FLAG and interrupt flag if it is enabled
							STAT |= 0x04;
							if (((STAT & 0x40) > 0)) { LYC_INT = true; }
						}
					}
				}

				// here LY=LYC will be asserted or cleared (but only if LY isnt 0 as that's a special case)
				if ((cycle == 4) && (LY != 0))
				{
					if (LY_inc == 1)
					{
						LYC_INT = false;
						STAT &= 0xFB;
					}
				}
				else if ((cycle == 6) && (LY != 0))
				{
					if ((LY == LYC) && !((STAT & 0x4) > 0))
					{
						// set STAT coincidence FLAG and interrupt flag if it is enabled
						STAT |= 0x04;
						if (((STAT & 0x40) > 0)) { LYC_INT = true; }
					}
				}

				cycle++;
			}
			else
			{
				STAT &= 0xFC;

				VBL_INT = LYC_INT = HBL_INT = OAM_INT = false;

				in_vblank[0] = true;

				LCD_was_off = true;

				LY = 0;
				cpu_LY[0] = LY;

				cycle = 0;
			}

			// assert the STAT IRQ line if the line went from zero to 1
			stat_line = VBL_INT | LYC_INT | HBL_INT | OAM_INT;

			if (stat_line && !stat_line_old)
			{
				if ((REG_FFFF[0] & 0x2) > 0) { FlagI[0] = true; }
				REG_FF0F[0] |= 0x02;		
			}

			stat_line_old = stat_line;

			// process latch delays
			//latch_delay();

			if (LYC_cd > 0)
			{
				LYC_cd--;
				if (LYC_cd == 0)
				{
					LYC = LYC_t;

					if (((LCDC & 0x80) > 0))
					{
						if (LY != LYC) { STAT &= 0xFB; LYC_INT = false; }
						else { STAT |= 0x4; LYC_INT = true; }
					}
				}
			}
		}

		// might be needed, not sure yet
		void latch_delay()
		{
			//BGP_l = BGP;
		}

		void render(uint32_t render_cycle)
		{
			// we are now in STAT mode 3
			// NOTE: presumably the first necessary sprite is fetched at sprite evaulation
			// i.e. just keeping track of the lowest x-value sprite
			if (render_cycle == 0)
			{
				/*
				OAM_access_read = false;
				OAM_access_write = true;
				VRAM_access_read = false;
				*/
				// window X is latched for the scanline, mid-line changes have no effect
				window_x_latch = window_x;

				OAM_scan_index = 0;
				read_case = 0;
				internal_cycle = 0;
				pre_render = true;
				pre_render_2 = true;
				tile_inc = 0;
				pixel_counter = -8;
				sl_use_index = 0;
				fetch_sprite = false;
				going_to_fetch = false;
				first_fetch = true;
				consecutive_sprite = -render_offset + 8;
				no_sprites = false;
				evaled_sprites = 0;
				window_pre_render = false;
				window_latch = ((LCDC & 0x20) > 0);

				total_counter = 0;

				// TODO: If Window is turned on midscanline what happens? When is this check done exactly?
				if ((window_started && window_latch) || (window_is_reset && !window_latch && (LY >= window_y_latch)))
				{
					window_y_tile_inc++;
					if (window_y_tile_inc == 8)
					{
						window_y_tile_inc = 0;
						window_y_tile++;
						window_y_tile %= 32;
					}
				}
				window_started = false;

				if (SL_sprites_index == 0) { no_sprites = true; }
				// it is much easier to process sprites if we order them according to the rules of sprite priority first
				if (!no_sprites) { reorder_and_assemble_sprites(); }
			}

			// before anything else, we have to check if windowing is in effect
			if (window_latch && !window_started && (LY >= window_y_latch) && (pixel_counter >= (window_x_latch - 7)) && (window_x_latch < 167))
			{
				/*
					Console.Write(LY);
					Console.Write(" ");
					Console.Write(cycle);
					Console.Write(" ");
					Console.Write(window_y_tile);
					Console.Write(" ");
					Console.Write(render_offset);
					Console.Write(" ");
					Console.Write(window_x_latch);
					Console.Write(" ");
					Console.WriteLine(pixel_counter);
				*/

				if (window_x_latch == 0)
				{
					// if the window starts at zero, we still do the first access to the BG
					// but then restart all over again at the window
					if ((render_offset % 7) <= 6)
					{
						read_case = 9;
					}
					else
					{
						read_case = 10;
					}
				}
				else
				{
					read_case = 4;
				}

				window_pre_render = true;

				window_counter = 0;
				render_counter = 0;

				window_x_tile = (uint32_t)floor((float)(pixel_counter - (window_x_latch - 7)) / 8.0);

				window_tile_inc = 0;
				window_started = true;
				window_is_reset = false;
			}

			if (!pre_render && !fetch_sprite)
			{
				// start shifting data into the LCD
				if (render_counter >= (render_offset + 8))
				{
					if (((tile_data_latch[2] & 0x20) > 0) && GBC_compat[0])
					{
						pixel = (tile_data_latch[0] & (1 << (render_counter % 8)) > 0) ? 1 : 0;
						pixel |= (tile_data_latch[1] & (1 << (render_counter % 8)) > 0) ? 2 : 0;
					}
					else
					{
						pixel = (tile_data_latch[0] & (1 << (7 - (render_counter % 8))) > 0) ? 1 : 0;
						pixel |= (tile_data_latch[1] & (1 << (7 - (render_counter % 8))) > 0) ? 2 : 0;
					}

					uint32_t ref_pixel = pixel;

					if (!GBC_compat[0])
					{
						if (((LCDC & 0x1) > 0))
						{
							pixel = (BGP >> (pixel * 2)) & 3;
						}
						else
						{
							pixel = 0;
						}
					}

					uint32_t pal_num = tile_data_latch[2] & 0x7;

					bool use_sprite = false;

					uint32_t s_pixel = 0;

					// now we have the BG pixel, we next need the sprite pixel
					if (!no_sprites)
					{
						bool have_sprite = false;
						uint32_t sprite_attr = 0;

						if (sprite_present_list[pixel_counter] == 1)
						{
							have_sprite = true;
							s_pixel = sprite_pixel_list[pixel_counter];
							sprite_attr = sprite_attr_list[pixel_counter];
						}

						if (have_sprite)
						{
							if (((LCDC & 0x2) > 0))
							{
								if (!((sprite_attr & 0x80) > 0))
								{
									use_sprite = true;
								}
								else if (ref_pixel == 0)
								{
									use_sprite = true;
								}

								if (!((LCDC & 0x1) > 0))
								{
									use_sprite = true;
								}

								// There is another priority bit in GBC, that can still override sprite priority
								if (((LCDC & 0x1) > 0) && ((tile_data_latch[2] & 0x80) > 0) && (ref_pixel != 0) && GBC_compat[0])
								{
									use_sprite = false;
								}
							}

							if (use_sprite)
							{
								pal_num = sprite_attr & 7;

								if (!GBC_compat[0])
								{
									pal_num = ((sprite_attr & 0x10) > 0) ? 1 : 0;

									if (((sprite_attr & 0x10) > 0))
									{
										pixel = (obj_pal_1 >> (s_pixel * 2)) & 3;
									}
									else
									{
										pixel = (obj_pal_0 >> (s_pixel * 2)) & 3;
									}
								}
							}
						}
					}

					// based on sprite priority and pixel values, pick a final pixel color
					if (GBC_compat[0])
					{
						if (use_sprite)
						{
							_vidbuffer[LY * 160 + pixel_counter] = (uint32_t)OBJ_palette[pal_num * 4 + s_pixel];
						}
						else
						{
							_vidbuffer[LY * 160 + pixel_counter] = (uint32_t)BG_palette[pal_num * 4 + pixel];
						}
					}
					else
					{
						if (use_sprite)
						{
							_vidbuffer[LY * 160 + pixel_counter] = (uint32_t)OBJ_palette[pal_num * 4 + pixel];
						}
						else
						{
							_vidbuffer[LY * 160 + pixel_counter] = (uint32_t)BG_palette[pixel];
						}
					}

					pixel_counter++;

					if (pixel_counter == 160)
					{
						read_case = 8;
						hbl_countdown = 2;
					}
				}
				else if (pixel_counter < 0)
				{
					pixel_counter++;
				}
				render_counter++;
			}

			if (!fetch_sprite)
			{
				if (!pre_render_2)
				{
					// before we go on to read case 3, we need to know if we stall there or not
					// Gekkio's tests show that if sprites are at position 0 or 1 (mod 8) 
					// then it takes an extra cycle (1 or 2 more t-states) to process them

					if (!no_sprites && (pixel_counter < 160))
					{
						for (uint32_t i = 0; i < SL_sprites_index; i++)
						{
							if ((pixel_counter >= (SL_sprites[i * 4 + 1] - 8)) &&
								(pixel_counter < (SL_sprites[i * 4 + 1])) &&
								!((evaled_sprites & (1 << i)) > 0))
							{
								going_to_fetch = true;
								fetch_sprite = true;
							}
						}
					}
				}

				switch (read_case)
				{
				case 0: // read a background tile
					if ((internal_cycle % 2) == 1)
					{
						// calculate the row number of the tiles to be fetched
						y_tile = ((uint32_t)floor((float)((uint32_t)scroll_y + LY) / 8.0)) % 32;

						temp_fetch = y_tile * 32 + (x_tile + tile_inc) % 32;
						tile_byte = VRAM[0x1800 + (((LCDC & 0x4) > 0) ? 1 : 0) * 0x400 + temp_fetch];
						tile_data[2] = VRAM[0x3800 + (((LCDC & 0x4) > 0) ? 1 : 0) * 0x400 + temp_fetch];
						VRAM_sel = ((tile_data[2] & 0x8) > 0) ? 1 : 0;

						BG_V_flip = ((tile_data[2] & 0x40) > 0) & GBC_compat[0];

						read_case = 1;
						if (!pre_render)
						{
							tile_inc++;
						}
					}
					break;

				case 1: // read from tile graphics (0)
					if ((internal_cycle % 2) == 1)
					{
						y_scroll_offset = (scroll_y + LY) % 8;

						if (BG_V_flip)
						{
							y_scroll_offset = 7 - y_scroll_offset;
						}

						if (((LCDC & 0x10) > 0))
						{
							tile_data[0] = VRAM[(VRAM_sel * 0x2000) + tile_byte * 16 + y_scroll_offset * 2];
						}
						else
						{
							// same as before except now tile uint8_t represents a signed byte
							if (((tile_byte & 0x80) > 0))
							{
								tile_byte -= 256;
							}
							tile_data[0] = VRAM[(VRAM_sel * 0x2000) + 0x1000 + tile_byte * 16 + y_scroll_offset * 2];
						}

						read_case = 2;
					}
					break;

				case 2: // read from tile graphics (1)
					if ((internal_cycle % 2) == 0)
					{
						pre_render_2 = false;
					}
					else
					{
						y_scroll_offset = (scroll_y + LY) % 8;

						if (BG_V_flip)
						{
							y_scroll_offset = 7 - y_scroll_offset;
						}

						if (((LCDC & 0x10) > 0))
						{
							// if LCDC somehow changed between the two reads, make sure we have a positive number
							if (tile_byte < 0)
							{
								tile_byte += 256;
							}
							tile_data[1] = VRAM[(VRAM_sel * 0x2000) + tile_byte * 16 + y_scroll_offset * 2 + 1];
						}
						else
						{
							// same as before except now tile uint8_t represents a signed byte
							if (((tile_byte & 0x80) > 0) && tile_byte > 0)
							{
								tile_byte -= 256;
							}
							tile_data[1] = VRAM[(VRAM_sel * 0x2000) + 0x1000 + tile_byte * 16 + y_scroll_offset * 2 + 1];
						}

						if (pre_render)
						{
							// here we set up rendering
							pre_render = false;

							render_counter = 0;
							latch_counter = 0;
							read_case = 0;
						}
						else
						{
							read_case = 3;
						}
					}
					break;

				case 3: // read from sprite data
					if ((internal_cycle % 2) == 1)
					{
						read_case = 0;
						latch_new_data = true;
					}
					break;

				case 4: // read from window data
					if ((window_counter % 2) == 1)
					{
						temp_fetch = window_y_tile * 32 + (window_x_tile + window_tile_inc) % 32;
						tile_byte = VRAM[0x1800 + (((LCDC & 0x40) > 0) ? 1 : 0) * 0x400 + temp_fetch];
						tile_data[2] = VRAM[0x3800 + (((LCDC & 0x40) > 0) ? 1 : 0) * 0x400 + temp_fetch];
						VRAM_sel = ((tile_data[2] & 0x8) > 0) ? 1 : 0;
						BG_V_flip = ((tile_data[2] & 0x40) > 0) & GBC_compat[0];

						window_tile_inc++;
						read_case = 5;
					}
					window_counter++;
					break;

				case 5: // read from tile graphics (for the window)
					if ((window_counter % 2) == 1)
					{
						y_scroll_offset = (window_y_tile_inc) % 8;

						if (BG_V_flip)
						{
							y_scroll_offset = 7 - y_scroll_offset;
						}

						if (((LCDC & 0x10) > 0))
						{
							tile_data[0] = VRAM[(VRAM_sel * 0x2000) + tile_byte * 16 + y_scroll_offset * 2];
						}
						else
						{
							// same as before except now tile uint8_t represents a signed byte
							if (((tile_byte & 0x80) > 0))
							{
								tile_byte -= 256;
							}
							tile_data[0] = VRAM[(VRAM_sel * 0x2000) + 0x1000 + tile_byte * 16 + y_scroll_offset * 2];
						}

						read_case = 6;
					}
					window_counter++;
					break;

				case 6: // read from tile graphics (for the window)
					if ((window_counter % 2) == 1)
					{
						y_scroll_offset = (window_y_tile_inc) % 8;

						if (BG_V_flip)
						{
							y_scroll_offset = 7 - y_scroll_offset;
						}

						if (((LCDC & 0x10) > 0))
						{
							// if LCDC somehow changed between the two reads, make sure we have a positive number
							if (tile_byte < 0)
							{
								tile_byte += 256;
							}
							tile_data[1] = VRAM[(VRAM_sel * 0x2000) + tile_byte * 16 + y_scroll_offset * 2 + 1];
						}
						else
						{
							// same as before except now tile uint8_t represents a signed byte
							if (((tile_byte & 0x80) > 0) && tile_byte > 0)
							{
								tile_byte -= 256;
							}
							tile_data[1] = VRAM[(VRAM_sel * 0x2000) + 0x1000 + tile_byte * 16 + y_scroll_offset * 2 + 1];
						}

						if (window_pre_render)
						{
							// here we set up rendering
							// unlike for the normal background case, there is no pre-render period for the window
							// so start shifting in data to the screen right away
							if (window_x_latch <= 7)
							{
								if (render_offset == 0)
								{
									read_case = 4;
								}
								else
								{
									read_case = 9 + render_offset - 1;
								}
								render_counter = 8 - render_offset;

								render_offset = 7 - window_x_latch;
							}
							else
							{
								render_offset = 0;
								read_case = 4;
								render_counter = 8;
							}

							latch_counter = 0;
							latch_new_data = true;
							window_pre_render = false;
						}
						else
						{
							read_case = 7;
						}
					}
					window_counter++;
					break;

				case 7: // read from sprite data
					if ((window_counter % 2) == 1)
					{
						read_case = 4;
						latch_new_data = true;
					}
					window_counter++;
					break;

				case 8: // done reading, we are now in phase 0
					pre_render = true;

					// the other interrupts appear to be delayed by 1 CPU cycle, so do the same here
					if (hbl_countdown > 0)
					{
						hbl_countdown--;

						if (hbl_countdown == 0)
						{
							OAM_access_read = true;
							OAM_access_write = true;
							VRAM_access_read = true;
							VRAM_access_write = true;
						}
						else
						{
							STAT &= 0xFC;
							STAT |= 0x00;

							if (((STAT & 0x8) > 0)) { HBL_INT = true; }
						}
					}
					break;

				case 9:
					// this is a degenerate case for starting the window at 0
					// kevtris' timing doc indicates an additional normal BG access
					// but this information is thrown away, so it's faster to do this then constantly check
					// for it in read case 0
					read_case = 4;
					break;
				case 10:
				case 11:
				case 12:
				case 13:
				case 14:
				case 15:
				case 16:
				case 17:
					read_case--;
					break;
				}
				internal_cycle++;

				if (latch_new_data)
				{
					latch_new_data = false;
					tile_data_latch[0] = tile_data[0];
					tile_data_latch[1] = tile_data[1];
					tile_data_latch[2] = tile_data[2];
				}
			}

			// every in range sprite takes 6 cycles to process
			// sprites located at x=0 still take 6 cycles to process even though they don't appear on screen
			// sprites above x=168 do not take any cycles to process however
			if (fetch_sprite)
			{
				if (going_to_fetch)
				{
					going_to_fetch = false;

					last_eval = 0;

					// at this time it is unknown what each cycle does, but we only need to accurately keep track of cycles
					for (uint32_t i = 0; i < SL_sprites_index; i++)
					{
						if ((pixel_counter >= (SL_sprites[i * 4 + 1] - 8)) &&
							(pixel_counter < (SL_sprites[i * 4 + 1])) &&
							!((evaled_sprites & (1 << i)) > 0))
						{
							sprite_fetch_counter += 6;
							evaled_sprites |= (1 << i);
							last_eval = SL_sprites[i * 4 + 1];
						}
					}

					// x scroll offsets the penalty table
					// there is no penalty if the next sprites to be fetched are within the currentfetch block (8 pixels)
					if (first_fetch || (last_eval >= consecutive_sprite))
					{
						if (((last_eval + render_offset) % 8) == 0) { sprite_fetch_counter += 5; }
						else if (((last_eval + render_offset) % 8) == 1) { sprite_fetch_counter += 4; }
						else if (((last_eval + render_offset) % 8) == 2) { sprite_fetch_counter += 3; }
						else if (((last_eval + render_offset) % 8) == 3) { sprite_fetch_counter += 2; }
						else if (((last_eval + render_offset) % 8) == 4) { sprite_fetch_counter += 1; }
						else if (((last_eval + render_offset) % 8) == 5) { sprite_fetch_counter += 0; }
						else if (((last_eval + render_offset) % 8) == 6) { sprite_fetch_counter += 0; }
						else if (((last_eval + render_offset) % 8) == 7) { sprite_fetch_counter += 0; }

						consecutive_sprite = (uint32_t)floor((double)(last_eval + render_offset) / 8.0) * 8 + 8 - render_offset;

						// special case exists here for sprites at zero with non-zero x-scroll. Not sure exactly the reason for it.
						if (last_eval == 0 && render_offset != 0)
						{
							sprite_fetch_counter += render_offset;
						}
					}

					total_counter += sprite_fetch_counter;

					first_fetch = false;
				}
				else
				{
					sprite_fetch_counter--;
					if (sprite_fetch_counter == 0)
					{
						fetch_sprite = false;
					}
				}
			}

		}

		void process_sprite()
		{
			uint32_t y;
			uint32_t VRAM_temp = (((SL_sprites[sl_use_index * 4 + 3] & 0x8) > 0) && GBC_compat[0]) ? 1 : 0;

			if ((SL_sprites[sl_use_index * 4 + 3] & 0x40) > 0)
			{
				if (((LCDC & 0x4) > 0))
				{
					y = LY - (SL_sprites[sl_use_index * 4] - 16);
					y = 15 - y;
					sprite_sel[0] = VRAM[(VRAM_temp * 0x2000) + (SL_sprites[sl_use_index * 4 + 2] & 0xFE) * 16 + y * 2];
					sprite_sel[1] = VRAM[(VRAM_temp * 0x2000) + (SL_sprites[sl_use_index * 4 + 2] & 0xFE) * 16 + y * 2 + 1];
				}
				else
				{
					y = LY - (SL_sprites[sl_use_index * 4] - 16);
					y = 7 - y;
					sprite_sel[0] = VRAM[(VRAM_temp * 0x2000) + SL_sprites[sl_use_index * 4 + 2] * 16 + y * 2];
					sprite_sel[1] = VRAM[(VRAM_temp * 0x2000) + SL_sprites[sl_use_index * 4 + 2] * 16 + y * 2 + 1];
				}
			}
			else
			{
				if (((LCDC & 0x4) > 0))
				{
					y = LY - (SL_sprites[sl_use_index * 4] - 16);
					sprite_sel[0] = VRAM[(VRAM_temp * 0x2000) + (SL_sprites[sl_use_index * 4 + 2] & 0xFE) * 16 + y * 2];
					sprite_sel[1] = VRAM[(VRAM_temp * 0x2000) + (SL_sprites[sl_use_index * 4 + 2] & 0xFE) * 16 + y * 2 + 1];
				}
				else
				{
					y = LY - (SL_sprites[sl_use_index * 4] - 16);
					sprite_sel[0] = VRAM[(VRAM_temp * 0x2000) + SL_sprites[sl_use_index * 4 + 2] * 16 + y * 2];
					sprite_sel[1] = VRAM[(VRAM_temp * 0x2000) + SL_sprites[sl_use_index * 4 + 2] * 16 + y * 2 + 1];
				}
			}

			if ((SL_sprites[sl_use_index * 4 + 3] & 0x20) > 0)
			{
				uint32_t b0, b1, b2, b3, b4, b5, b6, b7 = 0;
				for (uint32_t i = 0; i < 2; i++)
				{
					b0 = (sprite_sel[i] & 0x01) << 7;
					b1 = (sprite_sel[i] & 0x02) << 5;
					b2 = (sprite_sel[i] & 0x04) << 3;
					b3 = (sprite_sel[i] & 0x08) << 1;
					b4 = (sprite_sel[i] & 0x10) >> 1;
					b5 = (sprite_sel[i] & 0x20) >> 3;
					b6 = (sprite_sel[i] & 0x40) >> 5;
					b7 = (sprite_sel[i] & 0x80) >> 7;

					sprite_sel[i] = (uint8_t)(b0 | b1 | b2 | b3 | b4 | b5 | b6 | b7);
				}
			}
		}

		// normal DMA moves twice as fast in double speed mode on GBC
		// So give it it's own function so we can seperate it from PPU tick
		void DMA_tick()
		{
			// Note that DMA is halted when the CPU is halted
			if (DMA_start && !cpu_halted)
			{
				if (DMA_clock >= 4)
				{
					DMA_OAM_access = false;
					if ((DMA_clock % 4) == 1)
					{
						// the cpu can't access memory during this time, but we still need the ppu to be able to.
						DMA_start = false;
						// Gekkio reports that A14 being high on DMA transfers always represent WRAM accesses
						// So transfers nominally from higher memory areas are actually still from there (i.e. FF -> DF)
						uint8_t DMA_actual = DMA_addr;
						if (DMA_addr > 0xDF) { DMA_actual &= 0xDF; }
						DMA_byte = ReadMemory((uint32_t)((DMA_actual << 8) + DMA_inc));
						DMA_start = true;
					}
					else if ((DMA_clock % 4) == 3)
					{
						OAM[DMA_inc] = DMA_byte;

						if (DMA_inc < (0xA0 - 1)) { DMA_inc++; }
					}
				}

				DMA_clock++;

				if (DMA_clock == 648)
				{
					DMA_start = false;
					DMA_OAM_access = true;
				}
			}
		}

		// order sprites according to x coordinate
		// note that for sprites of equal x coordinate, priority goes to first on the list
		void reorder_and_assemble_sprites()
		{
			sprite_ordered_index = 0;

			// In CGB mode, sprites are ordered solely based on their position in OAM, so they are already ordered
			
			if (GBC_compat[0]) 
			{
				for (uint32_t j = 0; j < SL_sprites_index; j++)
				{
					sl_use_index = j;
					process_sprite();
					SL_sprites_ordered[sprite_ordered_index * 4] = SL_sprites[j * 4 + 1];
					SL_sprites_ordered[sprite_ordered_index * 4 + 1] = sprite_sel[0];
					SL_sprites_ordered[sprite_ordered_index * 4 + 2] = sprite_sel[1];
					SL_sprites_ordered[sprite_ordered_index * 4 + 3] = SL_sprites[j * 4 + 3];
					sprite_ordered_index++;
				}
			}
			else
			{
				for (int i = 0; i < 256; i++)
				{
					for (int j = 0; j < SL_sprites_index; j++)
					{
						if (SL_sprites[j * 4 + 1] == i)
						{
							sl_use_index = j;
							process_sprite();
							SL_sprites_ordered[sprite_ordered_index * 4] = SL_sprites[j * 4 + 1];
							SL_sprites_ordered[sprite_ordered_index * 4 + 1] = sprite_sel[0];
							SL_sprites_ordered[sprite_ordered_index * 4 + 2] = sprite_sel[1];
							SL_sprites_ordered[sprite_ordered_index * 4 + 3] = SL_sprites[j * 4 + 3];
							sprite_ordered_index++;
						}
					}
				}
			}

			bool have_pixel = false;
			uint8_t s_pixel = 0;
			uint8_t sprite_attr = 0;

			for (uint32_t i = 0; i < 160; i++)
			{
				have_pixel = false;
				for (uint32_t j = 0; j < SL_sprites_index; j++)
				{
					if ((i >= (SL_sprites_ordered[j * 4] - 8)) &&
						(i < SL_sprites_ordered[j * 4]) &&
						!have_pixel)
					{
						// we can use the current sprite, so pick out a pixel for it
						uint32_t t_index = i - (SL_sprites_ordered[j * 4] - 8);

						t_index = 7 - t_index;

						sprite_data[0] = (uint8_t)((SL_sprites_ordered[j * 4 + 1] >> t_index) & 1);
						sprite_data[1] = (uint8_t)(((SL_sprites_ordered[j * 4 + 2] >> t_index) & 1) << 1);

						s_pixel = (uint8_t)(sprite_data[0] + sprite_data[1]);
						sprite_attr = (uint8_t)SL_sprites_ordered[j * 4 + 3];

						// pixel color of 0 is transparent, so if this is the case we don't have a pixel
						if (s_pixel != 0)
						{
							have_pixel = true;
						}
					}
				}

				if (have_pixel)
				{
					sprite_present_list[i] = 1;
					sprite_pixel_list[i] = s_pixel;
					sprite_attr_list[i] = sprite_attr;
				}
				else
				{
					sprite_present_list[i] = 0;
				}
			}
		}

		void OAM_scan(uint32_t OAM_cycle)
		{
			// we are now in STAT mode 2
			// TODO: maybe stat mode 2 flags are set at cycle 0 on visible scanlines?
			if (OAM_cycle == 0)
			{
				OAM_access_read = false;
				OAM_access_write = false;

				OAM_scan_index = 0;
				SL_sprites_index = 0;
				write_sprite = 0;
			}

			// the gameboy has 80 cycles to scan through 40 sprites, picking out the first 10 it finds to draw
			// the following is a guessed at implmenentation based on how NES does it, it's probably pretty close
			if (OAM_cycle < 10)
			{
				// start by clearing the sprite table (probably just clears X on hardware, but let's be safe here.)
				SL_sprites[OAM_cycle * 4] = 0;
				SL_sprites[OAM_cycle * 4 + 1] = 0;
				SL_sprites[OAM_cycle * 4 + 2] = 0;
				SL_sprites[OAM_cycle * 4 + 3] = 0;
			}
			else
			{
				if (write_sprite == 0)
				{
					if (OAM_scan_index < 40)
					{
						uint32_t temp = DMA_OAM_access ? OAM[OAM_scan_index * 4] : (uint32_t)0xFF;
						// (sprite Y - 16) equals LY, we have a sprite
						if ((temp - 16) <= LY &&
							((temp - 16) + 8 + (((LCDC & 0x4) > 0) ? 8 : 0)) > LY)
						{
							// always pick the first 10 in range sprites
							if (SL_sprites_index < 10)
							{
								SL_sprites[SL_sprites_index * 4] = temp;

								write_sprite = 1;
							}
							else
							{
								// if we already have 10 sprites, there's nothing to do, increment the index
								OAM_scan_index++;
							}
						}
						else
						{
							OAM_scan_index++;
						}
					}
				}
				else
				{
					uint32_t temp2 = DMA_OAM_access ? OAM[OAM_scan_index * 4 + write_sprite] : (uint32_t)0xFF;
					SL_sprites[SL_sprites_index * 4 + write_sprite] = temp2;
					write_sprite++;

					if (write_sprite == 4)
					{
						write_sprite = 0;
						SL_sprites_index++;
						OAM_scan_index++;
					}
				}
			}
		}

		void color_compute_BG()
		{
			uint32_t R;
			uint32_t G;
			uint32_t B;

			if ((BG_bytes_index % 2) == 0)
			{
				R = (uint32_t)(BG_bytes[BG_bytes_index] & 0x1F);
				G = (uint32_t)(((BG_bytes[BG_bytes_index] & 0xE0) | ((BG_bytes[BG_bytes_index + 1] & 0x03) << 8)) >> 5);
				B = (uint32_t)((BG_bytes[BG_bytes_index + 1] & 0x7C) >> 2);
			}
			else
			{
				R = (uint32_t)(BG_bytes[BG_bytes_index - 1] & 0x1F);
				G = (uint32_t)(((BG_bytes[BG_bytes_index - 1] & 0xE0) | ((BG_bytes[BG_bytes_index] & 0x03) << 8)) >> 5);
				B = (uint32_t)((BG_bytes[BG_bytes_index] & 0x7C) >> 2);
			}

			uint32_t retR = ((R * 13 + G * 2 + B) >> 1) & 0xFF;
			uint32_t retG = ((G * 3 + B) << 1) & 0xFF;
			uint32_t retB = ((R * 3 + G * 2 + B * 11) >> 1) & 0xFF;

			BG_palette[BG_bytes_index >> 1] = (uint32_t)(0xFF000000 | (retR << 16) | (retG << 8) | retB);
		}

		void color_compute_OBJ()
		{
			uint32_t R;
			uint32_t G;
			uint32_t B;

			if ((OBJ_bytes_index % 2) == 0)
			{
				R = (uint32_t)(OBJ_bytes[OBJ_bytes_index] & 0x1F);
				G = (uint32_t)(((OBJ_bytes[OBJ_bytes_index] & 0xE0) | ((OBJ_bytes[OBJ_bytes_index + 1] & 0x03) << 8)) >> 5);
				B = (uint32_t)((OBJ_bytes[OBJ_bytes_index + 1] & 0x7C) >> 2);
			}
			else
			{
				R = (uint32_t)(OBJ_bytes[OBJ_bytes_index - 1] & 0x1F);
				G = (uint32_t)(((OBJ_bytes[OBJ_bytes_index - 1] & 0xE0) | ((OBJ_bytes[OBJ_bytes_index] & 0x03) << 8)) >> 5);
				B = (uint32_t)((OBJ_bytes[OBJ_bytes_index] & 0x7C) >> 2);
			}

			uint32_t retR = ((R * 13 + G * 2 + B) >> 1) & 0xFF;
			uint32_t retG = ((G * 3 + B) << 1) & 0xFF;
			uint32_t retB = ((R * 3 + G * 2 + B * 11) >> 1) & 0xFF;

			OBJ_palette[OBJ_bytes_index >> 1] = (uint32_t)(0xFF000000 | (retR << 16) | (retG << 8) | retB);
		}

		void Reset()
		{
			LCDC = 0;
			STAT = 0x80;
			scroll_y = 0;
			scroll_x = 0;
			LY = 0;
			LYC = 0;
			DMA_addr = 0;
			BGP = 0xFF;
			obj_pal_0 = 0;
			obj_pal_1 = 0;
			window_y = 0x0;
			window_x = 0x0;
			window_x_latch = 0xFF;
			window_y_latch = 0xFF;
			LY_inc = 1;
			no_scan = false;
			OAM_access_read = true;
			VRAM_access_read = true;
			OAM_access_write = true;
			VRAM_access_write = true;
			DMA_OAM_access = true;

			cycle = 0;
			LYC_INT = false;
			HBL_INT = false;
			VBL_INT = false;
			OAM_INT = false;

			stat_line = false;
			stat_line_old = false;

			window_counter = 0;
			window_pre_render = false;
			window_started = false;
			window_tile_inc = 0;
			window_y_tile = 0;
			window_x_tile = 0;
			window_y_tile_inc = 0;

			BG_bytes_inc = false;
			OBJ_bytes_inc = false;
			BG_bytes_index = 0;
			OBJ_bytes_index = 0;
			BG_transfer_byte = 0;
			OBJ_transfer_byte = 0;

			HDMA_src_hi = 0;
			HDMA_src_lo = 0;
			HDMA_dest_hi = 0;
			HDMA_dest_lo = 0;

			VRAM_sel = 0;
			BG_V_flip = false;
			HDMA_active = false;
			HDMA_mode = false;
			cur_DMA_src = 0;
			cur_DMA_dest = 0;
			HDMA_length = 0;
			HDMA_countdown = 0;
			HBL_HDMA_count = 0;
			last_HBL = 0;
			HBL_HDMA_go = false;
			HBL_test = false;
		}
	};
}