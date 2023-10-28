#include <LPC17xx.h>
#include <RTL.h>
#include <stdlib.h>
#include "stdio.h"
#include "uart.h"
#include "GLCD.h"
#include "led.h"
#include "delay.h"
#include "Controller.h"
#include "timer.h"
#include "GLCDRenderLIB.h"
#include <stdbool.h>
#include "GameCommons.h"

#define DEBUG_MSG(x) printf("%s", x)

void GameINIT(unsigned int GameIndex);
/*---- Helper Fucntions -----------------------------------------
*   Functions used to handle safe reading and writing 
*-------------------------------------------------------------------*/
uint8_t GetCurrentGameSTATUS()
{
	uint8_t state;
	os_mut_wait (&mutex_map_TEXEL, 0xffff);
	state = m_GAME_STATUS;
	os_mut_release (&mutex_map_TEXEL);
	return state;
}

void clearTEXEL(unsigned int x, unsigned int y, unsigned int val)
{
	GLCD_Bargraph(x*MAP_TEXEL_SIZE, y*MAP_TEXEL_SIZE, MAP_TEXEL_SIZE, MAP_TEXEL_SIZE, val);
}

uint16_t Get_TEXEL_CONTENT(uint8_t x, uint8_t y)
{
		uint16_t j = TC_UNKNOWN;
		if(x < MAP_SCRN_W && y < MAP_SCRN_H)
		{
			os_mut_wait (&mutex_map_TEXEL, 0xffff);
			j = map_TEXEL[x+y*MAP_SCRN_W];
			os_mut_release (&mutex_map_TEXEL);
		}
		return j;
}

uint16_t Get_TEXEL_CONTENT_16(uint16_t x, uint16_t y)
{
	  uint16_t j = TC_UNKNOWN;
		x/=MAP_TEXEL_W;
		y/=MAP_TEXEL_H;
		if(x < MAP_SCRN_W && y < MAP_SCRN_H)
		{
			os_mut_wait (&mutex_map_TEXEL, 0xffff);
			j = map_TEXEL[x+y*MAP_SCRN_W];
			os_mut_release (&mutex_map_TEXEL);
		}
		return j;
}

uint8_t PathExist_Screen_Space(uint16_t x, uint16_t y)
{
		return (!(Get_TEXEL_CONTENT_16(x, y)&TC_WALL));
}

void Set_TEXEL_CONTENT(uint8_t x, uint8_t y, uint16_t type)
{
		if(x < MAP_SCRN_W && y < MAP_SCRN_H)
		{
			os_mut_wait (&mutex_map_TEXEL, 0xffff);
			map_TEXEL[x+y*MAP_SCRN_W] |= type;
			if(type != TC_UPDATE)
				map_TEXEL[x+y*MAP_SCRN_W] |= TC_UPDATE;
			os_mut_release (&mutex_map_TEXEL);
		}
}

void Reset_TEXEL_CONTENT(uint8_t x, uint8_t y, uint16_t type)
{
		if(x < MAP_SCRN_W && y < MAP_SCRN_H)
		{
			os_mut_wait (&mutex_map_TEXEL, 0xffff);
			map_TEXEL[x+y*MAP_SCRN_W] &= (~type);
			if(type != TC_UPDATE)
				map_TEXEL[x+y*MAP_SCRN_W] |= TC_UPDATE;
			os_mut_release (&mutex_map_TEXEL);
		}
}

void Set_TEXEL_CONTENT_16(uint16_t x, uint16_t y, uint16_t type)
{
		x/=MAP_TEXEL_W;
		y/=MAP_TEXEL_H;
		if(x < MAP_SCRN_W && y < MAP_SCRN_H)
		{
			os_mut_wait (&mutex_map_TEXEL, 0xffff);
			map_TEXEL[x+y*MAP_SCRN_W] |= type;
			if(type != TC_UPDATE)
				map_TEXEL[x+y*MAP_SCRN_W] |= TC_UPDATE;
			os_mut_release (&mutex_map_TEXEL);
		}
}

void Reset_TEXEL_CONTENT_16(uint16_t x, uint16_t y, uint16_t type)
{
		x/=MAP_TEXEL_W;
		y/=MAP_TEXEL_H;
		if(x < MAP_SCRN_W && y < MAP_SCRN_H)
		{
			os_mut_wait (&mutex_map_TEXEL, 0xffff);
			map_TEXEL[x+y*MAP_SCRN_W] &= (~type);
			if(type != TC_UPDATE)
				map_TEXEL[x+y*MAP_SCRN_W] |= TC_UPDATE;
			os_mut_release (&mutex_map_TEXEL);
		}
}

uint8_t Get_Enemy_Property(uint8_t x, uint8_t y, uint16_t type)
{
	uint8_t i, cx, cy, cp;
	for(i=0;i<ENEMIES_MAX_QUANT;i++)
	{	
		os_mut_wait (&mutex_Enemies, 0xffff);
		cx = Enemies[i].x;
		cy = Enemies[i].y;
		cp = Enemies[i].property;
		os_mut_release (&mutex_Enemies);
		if(cx == x && cy == y && (cp&((1<<7)+((type&(7<<2))<<1)))) //if found, return its property
		{
			return cp;//property
		}
	}
	return 0;//Empty
}

void Kill_Enemies_U16(uint16_t bx, uint16_t by)
{
	uint8_t i, cx, cy, cp;
	
	bx/=MAP_TEXEL_W;
	by/=MAP_TEXEL_H;
	for(i=0;i<ENEMIES_MAX_QUANT;i++)
	{	
		os_mut_wait (&mutex_Enemies, 0xffff);
		cx = Enemies[i].x;
		cy = Enemies[i].y;
		cp = Enemies[i].property;
		os_mut_release (&mutex_Enemies);
		if(cx == bx && cy == by && (cp&(1<<7))) //if found, return its property
		{
			cp = (cp>>2)&3;//get current health
			cp = (cp==0)?0:(cp-1);
			if(cp <= 0)
			{
				os_mut_wait (&mutex_Enemies, 0xffff);
				Enemies[i].x = 0;
				Enemies[i].y = 0;
				cp = Enemies[i].property;//record the property for erasing the texture
				Enemies[i].property = 0;
				os_mut_release (&mutex_Enemies);
				Reset_TEXEL_CONTENT(cx, cy, ((cp>>4)&7)<<2);//erase on screen
			}else
			{
				os_mut_wait (&mutex_Enemies, 0xffff);
				Enemies[i].property &= (~(3<<2));//clear health
				Enemies[i].property += (cp<<2);//update health
				os_mut_release (&mutex_Enemies);
			}
		}
	}
}

void renderMenu(unsigned int i, unsigned char* s)
{
/*MENUs*/
		if(i == GM_MENU)
		{
				GLCD_SetTextColor(Black);
				sprintf((char*)s, "ESCAPE");
				GLCD_DisplayString_V(30, 270, 6, s);
				sprintf((char*)s, "THE");
				GLCD_DisplayString_V(30, 220, 6, s);
				sprintf((char*)s, "DUNGEON");
				GLCD_DisplayString_V(30, 170, 6, s);
				GLCD_Bargraph(140, 10, 190, 6, 1024);	
				GLCD_SetTextColor(White);
			  sprintf((char*)s, "______");
				GLCD_DisplayString_V(145, 20, 6, s);
				GLCD_SetTextColor(Black);
				sprintf((char*)s, "START>");
				GLCD_DisplayString_V(145, 20, 6, s);
				GLCD_Bargraph(10, 135, 3, 110, 1024);
				GLCD_Bargraph(47, 135, 3, 110, 1024);					
				GLCD_Bargraph(10, 135, 40, 3, 1024);

		}
		else if(i == GM_PAUSE)
		{
				GLCD_SetTextColor(Black);
				sprintf((char*)s, "PAUSE");
				GLCD_DisplayString_V(30, 270, 6, s);
				sprintf((char*)s, "TIME: [%02d:%02d]", (m_GAME_ACCTime/60)%60, m_GAME_ACCTime%60);
				GLCD_DisplayString_V(30, 170, 6, s);
				sprintf((char*)s, "CONTI>");
				GLCD_DisplayString_V(145, 20, 6, s);
				sprintf((char*)s, "REPLAY");
				GLCD_DisplayString_V(5, 20, 6, s);
				
				GLCD_Bargraph(10, 135, 3, 110, 1024);
				GLCD_Bargraph(47, 135, 3, 110, 1024);					
				GLCD_Bargraph(10, 135, 40, 3, 1024);
				GLCD_Bargraph(10, 0, 3, 110, 1024);
				GLCD_Bargraph(47, 0, 3, 110, 1024);					
				GLCD_Bargraph(10, 110, 40, 3, 1024);	
				GLCD_Bargraph(140, 10, 190, 6, 1024);				
		}else if(i == GM_END_F)
		{
				GLCD_SetTextColor(Black);
				sprintf((char*)s, "MISSION");
				GLCD_DisplayString_V(30, 270, 6, s);
				sprintf((char*)s, "FAILED");
				GLCD_DisplayString_V(130, 240, 6, s);
				sprintf((char*)s, "TIME: [%02d:%02d]", (m_GAME_ACCTime/60)%60, m_GAME_ACCTime%60);
				GLCD_DisplayString_V(30, 170, 6, s);
				sprintf((char*)s, "REPLAY");
				GLCD_DisplayString_V(5, 20, 6, s);
				GLCD_Bargraph(10, 0, 3, 110, 1024);
				GLCD_Bargraph(47, 0, 3, 110, 1024);					
				GLCD_Bargraph(10, 110, 40, 3, 1024);
				GLCD_Bargraph(140, 10, 190, 6, 1024);	
		}else if(i == GM_END_S)
		{
				GLCD_SetTextColor(Black);
				sprintf((char*)s, "MISSION");
				GLCD_DisplayString_V(30, 270, 6, s);
				sprintf((char*)s, "SUCCESS!");
				GLCD_DisplayString_V(110, 240, 6, s);
				sprintf((char*)s, "TIME: [%02d:%02d]", (m_GAME_ACCTime/60)%60, m_GAME_ACCTime%60);
				GLCD_DisplayString_V(30, 170, 6, s);
				sprintf((char*)s, "REPLAY");
				GLCD_DisplayString_V(5, 20, 6, s);
				GLCD_Bargraph(10, 0, 3, 110, 1024);
				GLCD_Bargraph(47, 0, 3, 110, 1024);					
				GLCD_Bargraph(10, 110, 40, 3, 1024);
				GLCD_Bargraph(140, 10, 190, 6, 1024);	
		}
}
/*---- 7 Tasks -----------------------------------------
*   Task_MenuUpdate 			:: handling menu and game transitions
*   Task_Render 					:: handling rendering  
*		Task_InputHandler			:: input handling
*		Task_CharMapUpdate		:: handling character collisions, rewards ...
*		Task_ProjectilesUpdate:: Update projectiles positions
*		Task_TrapUpdate				:: Update Traps		
*		Task_EnemyUpdate			:: Update Enemy positions & mechanisms
*-------------------------------------------------------------------*/

/*=================================Menu===================================*/
__task void Task_MenuUpdate()
{
	os_itv_set (50);
	for(;;)
	{
		uint8_t PB = 0, GM_state, cp, i, j;
		unsigned char *s = (unsigned char*)malloc(8 * sizeof(unsigned char));
		os_itv_wait();			
		os_mut_wait (&mutex_Hiro, 0xffff);
		cp = Hiro.property;
		os_mut_release (&mutex_Hiro);
		//update pushbutton
		PB = readPushBtn();

		GM_state = GetCurrentGameSTATUS();
		//compute game status
		srand(timer_read());
		if(GM_state == GM_MENU)
		{
			if(PB)
			{
				os_mut_wait (&mutex_map_TEXEL, 0xffff);
				m_GAME_STATUS = GM_INIT;
				m_GAME_startTime = timer_read()/TIME_TICK_SCALE/1000;
				m_GAME_ACCTime = 0;
				os_mut_release (&mutex_map_TEXEL);
			}
		}else if (GM_state == GM_INIT)
		{
			GameINIT(rand());//good luck
			m_GAME_STATUS = GM_RUNNING;
		}else if(GM_state == GM_PAUSE)
		{
			if(PB)
			{
				os_mut_wait (&mutex_map_TEXEL, 0xffff);
				m_GAME_STATUS = GM_RUNNING;
				m_GAME_startTime = timer_read()/TIME_TICK_SCALE/1000 - m_GAME_ACCTime;
				for(i =0; i<MAP_SCRN_W; i++)
					{
						for(j = 0; j<MAP_SCRN_H; j++)
						{
							map_TEXEL[i+j*MAP_SCRN_W] |= TC_UPDATE;
						}
					}
				DelayMilliseconds(100);
				os_mut_release (&mutex_map_TEXEL);
			}
		}else if(GM_state == GM_END_S)
		{
			
		}else if(GM_state == GM_END_F)
		{

		}else//running
		{
			if(PB)
			{
				os_mut_wait (&mutex_map_TEXEL, 0xffff);
				m_GAME_STATUS = GM_PAUSE;
				os_mut_release (&mutex_map_TEXEL);
			}
			
			if(((cp>>2)&3) == 0)
			{
				os_mut_wait (&mutex_map_TEXEL, 0xffff);
				m_GAME_STATUS = GM_END_F;//Mission Failed
				os_mut_release (&mutex_map_TEXEL);
			}
			
			if(cp & HIRO_PROPERTY_WIN)
			{
				os_mut_wait (&mutex_map_TEXEL, 0xffff);
				m_GAME_STATUS = GM_END_S;
				os_mut_release (&mutex_map_TEXEL);
			}
		}
		
		free(s);

		os_tsk_pass();
	}
}


/*=================================RENDER===================================*/
__task void Task_Render()
{
	for(;;)
	{
		unsigned char *s = (unsigned char*)malloc(8 * sizeof(unsigned char));
		uint8_t i, j, cp, enemy_p;
		uint16_t cx, cy, type;
		
		i = GetCurrentGameSTATUS();
		if(i!= screenClear)
		{
				GLCD_Clear(White);
				screenClear = i;
		}
		if(i == GM_RUNNING)
		{
				//draw Bullets
				GLCD_SetTextColor(Magenta);//bullet clr
				GLCD_SetBackColor(White);
				j = 0;//trakcing num of bullet			
			
				for(i = 0; i< BULLET_MAX_QUANT; i++)
				{
					os_mut_wait (&mutex_bullets, 0xffff);
					cp = Bullets[i].property;//exist 
					cx = Bullets[i].x;
					cy = Bullets[i].y;
					j = Bullet_quant;
					os_mut_release (&mutex_bullets);
					
					type = Get_TEXEL_CONTENT_16(cx, cy);//prevent rendering new bullet onto wall and player
					if((cp>>7)&&!((type&(TC_PLAYER|TC_WALL|TC_POTION))))//if exist, render it
					{
						GLCD_Bargraph(cx, cy, BULLET_W, BULLET_H, 1024);//render bullet
						//apply motion
						switch(cp&3)
						{
							case 0: 
								cx -= BULLET_SPEED;
								break;
							case 1:
								cy -= BULLET_SPEED;
								break;
							case 2:
								cx += BULLET_SPEED;
								break;
							case 3:
								cy += BULLET_SPEED;
								break;
						}
						type = Get_TEXEL_CONTENT_16(cx, cy);
						if(!(type&(TC_PLAYER|TC_WALL)))
							GLCD_Bargraph(cx, cy, BULLET_W, BULLET_H, 0);
					}
				}
				//indicate amount of bullet left
				for(i = 0; i< j; i++)
				{
					LED_set(BULLET_MAX_QUANT - i - 1);
				}
				for(j; j< BULLET_MAX_QUANT; j++)
				{
					LED_clear(BULLET_MAX_QUANT - j -1);
				}
				
				//GLCD_Clear(Blue);
				os_mut_wait (&mutex_Hiro, 0xffff);
				cx = Hiro.x;
				cy = Hiro.y;
				cp = Hiro.property;
				os_mut_release (&mutex_Hiro);
				
				//m_START_END[m_GAME_INDEX]
				os_mut_wait (&mutex_m_START_END, 0xffff);
				i = m_START_END[m_GAME_INDEX][2];
				j = m_START_END[m_GAME_INDEX][3];
				os_mut_release (&mutex_m_START_END);
				GLCD_SetTextColor(Purple);
				GLCD_Ptergraph(300,220,10,j- cy,i-cx,1);
						
				GLCD_SetTextColor(Black);//wall
				GLCD_SetBackColor(White);//path
				for(i =0; i<(MAP_SCRN_W - 1); i++)
				{
					for(j = 0; j<MAP_SCRN_H; j++)
					{
						type = Get_TEXEL_CONTENT(i, j);
						
						if((type & TC_UPDATE))
						{
							Reset_TEXEL_CONTENT(i, j, TC_UPDATE);
						
							enemy_p = (((type&TC_ZOMBIE)|(type&TC_GHOST))|type&TC_RAT);//if enemyy exist
							if(type&TC_WALL)//WALL
							{
								draw_TEXEL(i, j, 0, TC_WALL);
								//clearTEXEL(i, j, 1024);
							}
							else if(type&TC_PLAYER)//PLAYER
							{
								if(cp&HIRO_PROPERTY_INVINCIBLE)
								{
									clearTEXEL(i,j, 0);
									DelayMilliseconds(200);
								}
								draw_TEXEL(i, j, cp, TC_PLAYER);
							}
							else if(!enemy_p)//PATH
							{
								clearTEXEL(i,j, 0);
							}
							
							if((enemy_p)&&!(type&TC_PLAYER))//ENEMIES
							{
									if(type&TC_ZOMBIE)
									{
										enemy_p = Get_Enemy_Property(i, j, TC_ZOMBIE);
										draw_TEXEL(i, j, (enemy_p&3), TC_ZOMBIE);
									}
									else if(type&TC_GHOST)
									{
										draw_TEXEL(i, j, 0, TC_GHOST);
									}
									else if(type&TC_RAT)
									{
										enemy_p = Get_Enemy_Property(i, j, TC_RAT);
										draw_TEXEL(i, j, (enemy_p&3), TC_RAT);
									}
							}else if(!(type&TC_PLAYER))
							{
									if(type&TC_POTION)
									{
										draw_TEXEL(i, j, 0, TC_POTION);
									}
									else if(type&TC_PORTAL)
									{
										draw_TEXEL(i, j, 0, TC_PORTAL);
									}
									else if(type&TC_TRAP_NEEDLE)
									{
										GLCD_SetTextColor(Red);//on
										GLCD_SetBackColor(LightGrey);//off
										if(type & TC_TRAP_NEEDLE_STATUS)
										{
											draw_TEXEL(i*MAP_TEXEL_W, j*MAP_TEXEL_W, 0, TC_TRAP_NEEDLE_ON);
										}else
										{
											draw_TEXEL(i*MAP_TEXEL_W, j*MAP_TEXEL_W, 0, TC_TRAP_NEEDLE_OFF);
										}
										GLCD_SetTextColor(Black);//wall
										GLCD_SetBackColor(White);//path
									}
							}
						} //end of updating if statement
					}//End of For loop 
				}
				
				//draw Healthbar
				GLCD_SetTextColor(Red);//health bar
				GLCD_SetBackColor(DarkGrey);//backgrnd
				for(i = 0; i< HIRO_MAX_HEALTH; i++)
				{
					GLCD_Bargraph(305, 10 + i*25, 10, 20, (i<((cp>>2)&3))?1024:0);//toggle health
				}
				
				//display timer 
				m_GAME_ACCTime = (timer_read()/TIME_TICK_SCALE/1000-m_GAME_startTime);
				GLCD_SetTextColor(Maroon);
				GLCD_SetBackColor(White);
				sprintf((char*)s, "%02d:%02d", (m_GAME_ACCTime/60)%60, m_GAME_ACCTime%60);
				GLCD_DisplayString_V(110, 300, 6, s);
			
		}else
			renderMenu(i, s);
		
		free(s);
		os_tsk_pass();
	}
}

/*=================================INPUT HANDLER===================================*/
__task void Task_InputHandler()
{
	os_itv_set (INPUT_UPDATE_PERIOD);
	for(;;)
	{
		uint16_t potentioReading;
		uint8_t joyCommand;
		uint8_t texel_property, triggerBullet = 0; 
		int8_t x, y, p;
		int delta = 0;
		os_itv_wait ();
		
		if(GetCurrentGameSTATUS() == GM_RUNNING)
		{
					//read current position
					os_mut_wait (&mutex_Hiro, 0xffff);
					x = Hiro.x;
					y = Hiro.y;
					p = Hiro.property;
					os_mut_release (&mutex_Hiro);

					//obtain current joystick 
					joyCommand = readJoystick();				
					
					//parse joystick
					if(joyCommand != JS_IDLE)
					{
						switch ((joyCommand) & (~JS_PRESSED))//Check direction
						{
							case JS_UP:
								y--;
								break;
							case JS_DOWN:
								y++;
								break;
							case JS_LEFT:
								x--;
								break;
							case JS_RIGHT:
								x++;
								break;
							default:
								break;
						}
						if(joyCommand & JS_PRESSED)
						{
							triggerBullet = 1;
						}
					}
					
					//potentiometer => Character rotation
					potentioReading = readPotentiometer();
					delta = (potentioReading-prevPotentio); 
					if(((delta<0)?(-delta):delta)>POTENTIO_SENSITIVITY)
					{
						delta = Hiro.property+(delta<0?1:-1);
						delta = delta<0?3:delta;
						p &=(~3);
						p += delta%4;
						prevPotentio = potentioReading;
					}
					
					//check if the new position is valid, (walkable check)
					texel_property = Get_TEXEL_CONTENT(x, y);
					if(!(texel_property&TC_WALL))//Not wall
					{
							//if valid, update position
							os_mut_wait (&mutex_Hiro, 0xffff);
							Reset_TEXEL_CONTENT(Hiro.x, Hiro.y, TC_PLAYER);
							Hiro.x = x;
							Hiro.y = y;
							Hiro.property = p;
							os_mut_release (&mutex_Hiro);
							Set_TEXEL_CONTENT(x, y, TC_PLAYER);
					}
					
					//Bullet Releasing check
					if(triggerBullet && Bullet_quant > 0)
					{
						os_mut_wait (&mutex_bullets, 0xffff);
						//NOTE::Here texel_property is reused as for loop counter
						for(texel_property = 0; texel_property < BULLET_MAX_QUANT; texel_property++)
						{
							//if bullet is not busy
							if(Bullets[texel_property].property == 0)
							{
								Bullets[texel_property].x = x*MAP_TEXEL_W+(MAP_TEXEL_SIZE-BULLET_W)/2;
								Bullets[texel_property].y = y*MAP_TEXEL_H+(MAP_TEXEL_SIZE-BULLET_H)/2;
								Bullets[texel_property].property = (1<<7)|p;//enable the bullet
								Bullet_quant--;
								break;
							}
						}
						os_mut_release (&mutex_bullets); 
					}
		}

		
		//End of one cycle
		os_tsk_pass();
	}
}


/*=================================CHAR MAP===================================*/
__task void Task_CharMapUpdate()
{

	for(;;)
	{
		uint8_t x, y, p;
		uint16_t texel_content;
		uint32_t cur_time;
		
		if(GetCurrentGameSTATUS() == GM_RUNNING)
		{
				//Compute Character health
				//read current position
				os_mut_wait (&mutex_Hiro, 0xffff);
				x = Hiro.x;
				y = Hiro.y;
				p = Hiro.property;
				os_mut_release (&mutex_Hiro);
				texel_content = Get_TEXEL_CONTENT(x, y);
				
				//process texel contents
				cur_time = timer_read()/TIME_TICK_SCALE;
				if(p&HIRO_PROPERTY_INVINCIBLE)//invincible protection mode
				{
					if((cur_time - m_Hiro_LastDMGTime)> INVINCIBLE_DURATION)
					{
						p &= (~HIRO_PROPERTY_INVINCIBLE);//exit invincible mode
					}
				}
				else
				{
					if(texel_content&(TC_ZOMBIE|TC_GHOST|TC_RAT|TC_TRAP_NEEDLE_STATUS))//collided with enemy, [-1 health]
					{
							p = ((p>>2)&3)<=0?p:(p-(1<<2));// -1hp
							p += HIRO_PROPERTY_INVINCIBLE;//apply invincibility
							m_Hiro_LastDMGTime = cur_time;
					}
				}
				
				//-----------Rewards
				if(texel_content&TC_POTION)
				{
					p = (((p>>2)&3)<HIRO_MAX_HEALTH)?(p+(1<<2)):p;
					Reset_TEXEL_CONTENT(x, y, TC_POTION);//delete the portion
				}
				else if((texel_content&TC_PORTAL))//Teleporting portal
				{
					os_mut_wait (&mutex_Portals, 0xffff);
					if(Portals[0].x == x && !Portal_InUse)
					{
						Portal_InUse = 2;
						os_mut_wait (&mutex_Hiro, 0xffff);
						Reset_TEXEL_CONTENT(Hiro.x, Hiro.y, TC_PLAYER);
						Hiro.x = Portals[1].x;
						Hiro.y = Portals[1].y;
						Set_TEXEL_CONTENT(Hiro.x, Hiro.y, TC_PLAYER);
						os_mut_release (&mutex_Hiro);
					}else if(!Portal_InUse)
					{
						Portal_InUse = 1;
						os_mut_wait (&mutex_Hiro, 0xffff);
						Reset_TEXEL_CONTENT(Hiro.x, Hiro.y, TC_PLAYER);
						Hiro.x = Portals[0].x;
						Hiro.y = Portals[0].y;
						Set_TEXEL_CONTENT(Hiro.x, Hiro.y, TC_PLAYER);
						os_mut_release (&mutex_Hiro);
					}
					os_mut_release (&mutex_Portals);
					
				}else
				{
					Portal_InUse = false;
				}
				
				
				//check the dist away from the end goal
				os_mut_wait (&mutex_m_START_END, 0xffff);
				x = m_START_END[m_GAME_INDEX][2]-x;
				y = m_START_END[m_GAME_INDEX][3]-y;
				os_mut_release (&mutex_m_START_END);
				if((x+y)==0)
				{
					p += HIRO_PROPERTY_WIN;
				}
				
				os_mut_wait (&mutex_Hiro, 0xffff);
				Hiro.property = p;
				os_mut_release (&mutex_Hiro);
				
				
		}//END- GM_RUNNING
			
		//End of one cycle
		os_tsk_pass();
	}
}


/*=================================PROJECTILES CALC===================================*/
__task void Task_ProjectilesUpdate()
{
	os_itv_set (10);
	for(;;)
	{
		uint8_t i, j;
		uint16_t bx, by;
		//projectile collision check
		os_itv_wait ();
		
		if(GetCurrentGameSTATUS() == GM_RUNNING)
		{
					//Here isWalkable is used as for loop counter
					for(i = 0; i < BULLET_MAX_QUANT; i++)
					{
							//copy data
							os_mut_wait (&mutex_bullets, 0xffff);
							j = Bullets[i].property;
							bx = Bullets[i].x;
							by = Bullets[i].y;
							os_mut_release (&mutex_bullets); 
							
							//check
							//if bullet is a active
							if(j != 0)//|DIRECTION: ..00:x++ ..01:y++ ..10:x-- ..11:y-- 
							{
								Set_TEXEL_CONTENT_16(bx, by, TC_UPDATE);//update rendering
								//apply motion
								switch(j&3)
								{
									case 0: 
										bx += BULLET_SPEED;
										break;
									case 1:
										by += BULLET_SPEED;
										break;
									case 2:
										bx -= BULLET_SPEED;
										break;
									case 3:
										by -= BULLET_SPEED;
										break;
								}
								
								j = 1;
								j = Get_TEXEL_CONTENT_16(bx, by);
								//collision check
								if((j&TC_GHOST)|(j&TC_ZOMBIE)|(j&TC_RAT))
								{
										//Set_TEXEL_CONTENT_16(bx, by, TC_14_15_SPECIAL);
										Kill_Enemies_U16(bx, by);
										//return bullet
										os_mut_wait (&mutex_bullets, 0xffff);
										Bullets[i].property = 0;
										Bullets[i].x = 0;
										Bullets[i].y = 0;
										Bullet_quant++;
										os_mut_release (&mutex_bullets); 
								}
								else if(j&TC_WALL)
								{
										//return bullet
										os_mut_wait (&mutex_bullets, 0xffff);
										Bullets[i].property = 0;
										Bullets[i].x = 0;
										Bullets[i].y = 0;
										Bullet_quant++;
										os_mut_release (&mutex_bullets); 
								}
								else
								{
										os_mut_wait (&mutex_bullets, 0xffff);
										Bullets[i].x = bx;
										Bullets[i].y = by;
										os_mut_release (&mutex_bullets); 
								}
								Set_TEXEL_CONTENT_16(bx, by, TC_UPDATE);//update rendering
							}
					}
		}//END - of GM_RUNNING
		
		os_tsk_pass();
	}
}


/*=================================TRAP===================================*/
__task void Task_TrapUpdate()
{

	for(;;)
	{
		uint8_t i, tx, ty, tp;
		uint16_t cur_time;
		
		cur_time = timer_read()/TIME_TICK_SCALE/TIME_TICK_TRAP_SCALE;
		if((cur_time - prev_time_needle) > TRAP_SPIKE_DURATION)
		{
				for (i =0; i<TRAPS_MAX_QUANT; i++)
				{
					os_mut_wait (&mutex_Traps, 0xffff);
					tx = Traps[i].x;
					ty = Traps[i].y;
					tp = Traps[i].property;
					os_mut_release (&mutex_Traps);
					
					if((tp&1)&&(tp&(1<<7)))
					{
						
							//toggle
							if(tp&2)//if it was on
							{
								Reset_TEXEL_CONTENT(tx, ty, TC_TRAP_NEEDLE_STATUS);
								tp -= 2;
							}else
							{
								Set_TEXEL_CONTENT(tx, ty, TC_TRAP_NEEDLE_STATUS);
								tp += 2;
							}
						
									
						os_mut_wait (&mutex_Traps, 0xffff);
						Traps[i].property = tp;
						os_mut_release (&mutex_Traps);
					}

				}
				prev_time_needle = cur_time;
		}
		
		
		os_tsk_pass();
	
	}
}



/*=================================ENEMY===================================*/
__task void Task_EnemyUpdate()
{
	os_itv_set (ENEMY_UPDATE_ITV);
	for(;;)
	{	
		uint8_t i, ex, ey, ep, valid, enemy_type, hitByBullet;
		uint16_t map_texel_type;
		os_itv_wait ();
		if(GetCurrentGameSTATUS() == GM_RUNNING)
		{
					for(i = 0; i< ENEMIES_MAX_QUANT; i++)
					{
						//read current position
						os_mut_wait (&mutex_Enemies, 0xffff);
						ex = Enemies[i].x;
						ey = Enemies[i].y;
						ep = Enemies[i].property;
						os_mut_release (&mutex_Enemies);
						
						if(ep>>7)//If this enemy is alived
						{
							valid = 1;
							hitByBullet = (Get_TEXEL_CONTENT(ex, ey)&TC_14_15_SPECIAL)>>14;
							
							//update position
							switch(ep&3)//filter first 2 bits to determine where to go
							{
								case 0: 
									ex += ENEMY_MOVING_SPEED;
									break;
								case 1:
									ey += ENEMY_MOVING_SPEED;
									break;
								case 2:
									ex -= ENEMY_MOVING_SPEED;
									break;
								case 3:
									ey -= ENEMY_MOVING_SPEED;
									break;
							}
							
							//type filter & position check & apply game mechanism:
							map_texel_type = Get_TEXEL_CONTENT(ex, ey);
							enemy_type = ((ep>>4)&7)<<2;//filter enemy types & reposition to match texel_content format				
							srand(timer_read());
							switch(enemy_type)
							{
								case TC_ZOMBIE: 
										if((map_texel_type&TC_WALL)||(rand()%4==0))//wall: zombie will rotate 
										{
											ep = (((ep&3)+1)%4)|(ep&(~3));//CW rotation
											valid = 2;
										}
										break;
									
								case TC_GHOST:
									{
										if(map_texel_type&TC_WALL)//wall: ghost will rotate 
										{
											ep = (((rand())%4)|(ep&(~3)));//CW rotation
											valid = 2;
										}
										break;
									}
								case TC_RAT:
									{
										if(map_texel_type&TC_WALL)//wall: rat will rotate 
										{
											ep = (((ep&3)+1)%4)|(ep&(~3));//CW rotation
											valid = 2;
										}
										break;
									}
							}
							if(hitByBullet!=0)
							{
								valid = 4;
							}
							//update data
							os_mut_wait (&mutex_Enemies, 0xffff);
							if(valid==1||valid==3)
							{
								Reset_TEXEL_CONTENT(Enemies[i].x, Enemies[i].y, enemy_type);
								Enemies[i].x = ex;
								Enemies[i].y = ey;
								Set_TEXEL_CONTENT(ex, ey, enemy_type);
							}
							if(valid==2||valid==3)
							{
								Enemies[i].property = ep;//apply
							}
							os_mut_release (&mutex_Enemies);
						}
					}
		}//END of GM_RUNNING
		os_tsk_pass();
	}
}




/*---- Mains -----------------------------------------
*   start_tasks :: initialize tasks & mutexes
*   init :: initialize game maps & hardwares
*		main :: the main body
*-------------------------------------------------------------------*/
void start_tasks()
{
	os_mut_init (&mutex_Hiro);
	os_mut_init (&mutex_map_TEXEL);
	os_mut_init (&mutex_bullets);
	os_mut_init (&mutex_Enemies);
	os_mut_init (&mutex_m_START_END);
	os_mut_init (&mutex_input);
	os_mut_init (&mutex_Portals);
	os_mut_init (&mutex_Traps);
	
	//
	os_tsk_create(Task_MenuUpdate, 1);
	os_tsk_create(Task_Render, 1);
	os_tsk_create(Task_InputHandler, 1);
	os_tsk_create(Task_CharMapUpdate,1);
	os_tsk_create(Task_ProjectilesUpdate, 1);
	os_tsk_create(Task_TrapUpdate, 1);
	os_tsk_create(Task_EnemyUpdate, 1);
	os_tsk_delete_self();
}

void GameINIT(unsigned int GameIndex)
{
	uint8_t i, j, isWall, random, counter_enemies = 0, counter_traps = 0;
	
	//lock everything
	os_mut_wait (&mutex_Hiro, 0xffff);
	os_mut_wait (&mutex_map_TEXEL, 0xffff);
	os_mut_wait (&mutex_bullets, 0xffff);
	os_mut_wait (&mutex_Enemies, 0xffff);
	os_mut_wait (&mutex_m_START_END, 0xffff);
	os_mut_wait (&mutex_input, 0xffff);
	os_mut_wait (&mutex_Portals, 0xffff);
	os_mut_wait (&mutex_Traps, 0xffff);
	
	//randomize game scenes
	srand(123);
	m_GAME_INDEX = GameIndex%MAP_VARIATION;
	
	//Game Objs Settings
	//Hiro 's initial position
	Hiro.x = m_START_END[m_GAME_INDEX][0];
	Hiro.y = m_START_END[m_GAME_INDEX][1];
	Hiro.property = 0 + (HIRO_MAX_HEALTH<<2) + (1<<7);//3health
	prevPotentio = readPotentiometer();
	//Load Map:
	map_TEXEL[Hiro.x + Hiro.y*MAP_SCRN_W] = TC_PLAYER;
	
		//assign rewards
	for (i=0; i< REWARDS_MAX_QUANT; i++)
	{
		if(i<2)//portal
		{
				Portals[i].x = Temp_rewards_pos[m_GAME_INDEX][i*3];
				Portals[i].y = Temp_rewards_pos[m_GAME_INDEX][i*3+1];
				Portals[i].property = Temp_rewards_pos[m_GAME_INDEX][i*3+2]>>5+1<<7;//save types + enable the reward
		}
		map_TEXEL[Temp_rewards_pos[m_GAME_INDEX][i*3]+Temp_rewards_pos[m_GAME_INDEX][i*3+1]*MAP_SCRN_W] |=  Temp_rewards_pos[m_GAME_INDEX][i*3+2];
	}
			
	for(i =0; i<MAP_SCRN_W; i++)
	{
		for(j = 0; j<MAP_SCRN_H; j++)
		{
			srand(timer_read()+i*535*j);
			isWall = (MAP[m_GAME_INDEX][j]&(1<<i))?TC_WALL:TC_PATH;
			map_TEXEL[i+j*MAP_SCRN_W] |= isWall;
			map_TEXEL[i+j*MAP_SCRN_W] |= TC_UPDATE;
			if(!isWall && i!=m_START_END[m_GAME_INDEX][0] && j!=m_START_END[m_GAME_INDEX][1] && i!= m_START_END[m_GAME_INDEX][2] && j!= m_START_END[m_GAME_INDEX][3])
			{
				//randomly generate enemies
				if( rand()%10 == 0 && counter_enemies < ENEMIES_MAX_QUANT)
				{
					Enemies[counter_enemies].x = i;
					Enemies[counter_enemies].y = j;
					random  = (rand()%3) +2;//max 3 types //random type
					Enemies[counter_enemies].property = (1<<(random+2))+(1<<7)+((random-1)<<2); //random type + active + health
					map_TEXEL[i+j*MAP_SCRN_W] |= 1<<random;
					random  = (rand())%4;//4 orientations
					Enemies[counter_enemies].property |= random;// + random orientation
					counter_enemies++;
				}
				else if( rand()%29 == 0 && counter_traps< TRAPS_MAX_QUANT &&!(map_TEXEL[i+j*MAP_SCRN_W]&Temp_rewards_pos[m_GAME_INDEX][2])&&!(map_TEXEL[i+j*MAP_SCRN_W]&Temp_rewards_pos[m_GAME_INDEX][5]))
				{
						//assign traps
						Traps[counter_traps].x = i;
						Traps[counter_traps].y = j;
						Traps[counter_traps].property = 1 + (1<<7);//needle traps + valid trap
						map_TEXEL[Traps[counter_traps].x+Traps[counter_traps].y*MAP_SCRN_W] |= TC_TRAP_NEEDLE_OFF;
						counter_traps++;
				}
			}			
			
		}
	}
	

	GLCD_Clear(White);
	//release everything
	os_mut_release (&mutex_Hiro);
	os_mut_release (&mutex_map_TEXEL);
	os_mut_release (&mutex_bullets);
	os_mut_release (&mutex_Enemies);
	os_mut_release (&mutex_m_START_END);
	os_mut_release (&mutex_input);
	os_mut_release (&mutex_Portals);
	os_mut_release (&mutex_Traps);
}

void init()
{
	LED_setup();
	initJoystick();
	initPotentiometer();
	initPushBtn();
	GLCD_Init();
	timer_setup();
	GLCD_Clear(White);
}

int main( void ) 
{
	init();
	DEBUG_MSG(" ");
	os_sys_init(start_tasks);
}
