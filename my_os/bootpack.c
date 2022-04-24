/* bootpackのメイン */

#include "bootpack.h"
#include <stdio.h>
void make_window8(unsigned char *buf, int xsize, int ysize, char *title);
void putfonts8_asc_sht(struct SHEET *sht,int x,int y,int c,int b,char *s,int l);
void HariMain(void)
{
	struct BOOTINFO *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
	struct FIFO32 fifo;
	char s[40];
	char ss[40];
	int fifobuf[128];
	struct TIMER *timer,*timer2,*timer3;
	int mx, my, i;
	int j,num,lnum;
	unsigned int memtotal;
	unsigned int page_list[10]={0};
	struct MOUSE_DEC mdec;
	
	struct SHTCTL *shtctl;
	struct SHEET *sht_back, *sht_mouse,*sht_win;
	unsigned char *buf_back, buf_mouse[256],*buf_win;


	init_gdtidt();
	init_pic();
	io_sti(); // IDT/PIC的初始化已经完成，于是开放CPU的中断 


	init_pit();
	fifo32_init(&fifo,128,fifobuf);
	init_keyboard(&fifo,256);
	enable_mouse(&fifo,512,&mdec);
	io_out8(PIC0_IMR, 0xf8); // 开放PIC1和键盘中断(11111001) 
	io_out8(PIC1_IMR, 0xef); // 开放鼠标中断(11101111) 

	timer = timer_alloc();
	timer_init(timer,&fifo,10);
	timer_settime(timer,1000);
	timer2 = timer_alloc();
	timer_init(timer2,&fifo,3);
	timer_settime(timer2,300);
	timer3 = timer_alloc();
	timer_init(timer3,&fifo,1);
	timer_settime(timer3,50);


	memtotal = memtest(0x00400000, 0xbfffffff); //测试内存大小
	buddy_alloc_t *buddy = buddy_init(MEMMAN_ADDR,0x00400000,memtotal); //初始化内存管理模块
	// memman_init(memman);
	// memman_free(memman, 0x00001000, 0x0009e000); /* 0x00001000 - 0x0009efff */
	// memman_free(memman, 0x00400000, memtotal - 0x00400000);


	init_palette();
	shtctl = shtctl_init(buddy, binfo->vram, binfo->scrnx, binfo->scrny); //初始化图层
	sht_back  = sheet_alloc(shtctl); //背景层
	sht_mouse = sheet_alloc(shtctl); //鼠标层
	// sht_win = sheet_alloc(shtctl);
	buf_back = (unsigned char *)buddy_alloc(buddy, binfo->scrnx * binfo->scrny);
	buf_win = (unsigned char *) buddy_alloc(buddy,160*52);
	sheet_setbuf(sht_back, buf_back, binfo->scrnx, binfo->scrny, -1); // 没有透明色 
	sheet_setbuf(sht_mouse, buf_mouse, 16, 16, 99); // 透明色号99 
	// sheet_setbuf(sht_win,buf_win,160,52,-1);
	init_screen8(buf_back, binfo->scrnx, binfo->scrny);
	init_mouse_cursor8(buf_mouse, 99); // 背景色号99 
	make_window8(buf_win,160,68,"counter");
	// putfonts8_asc(buf_win, 160,24,28,COL8_000000,"Welcome to");
	// putfonts8_asc(buf_win, 160, 24, 44, COL8_000000,"  Sipalaki-OS!");
	sheet_slide(sht_back, 0, 0);
	mx = (binfo->scrnx - 16) / 2; // 按显示在画面中央来计算坐标 
	my = (binfo->scrny - 28 - 16) / 2;
	sheet_slide(sht_mouse, mx, my);
	// sheet_slide(sht_win,80,72);
	sheet_updown( sht_back,  0);
	// sheet_updown( sht_win, 1);
	sheet_updown(sht_mouse, 2);
	sprintf(s, "(%3d, %3d)", mx, my);
	putfonts8_asc(buf_back, binfo->scrnx, 0, 0, COL8_FFFFFF, s);
	sprintf(s, "memory %dKB free : %dKB", memtotal / (1024), buddy_status(buddy) / 1024);// 显示内存情况
	putfonts8_asc(buf_back, binfo->scrnx, 0, 32, COL8_FFFFFF, s);
	sheet_refresh(sht_back, 0, 0, binfo->scrnx, 48); // 刷新文字 

	num = 0;
	for (;;) {
		// sprintf(s,"%010d",timerctl.count);
		// putfonts8_asc_sht(sht_win,40,28,COL8_000000,COL8_C6C6C6,s,10);
		io_cli();
		if (fifo32_status(&fifo) == 0) {
			io_sti();
		} else {
				i = fifo32_get(&fifo);
				io_sti();
				if (256<=i && i <=511){
				sprintf(s, "%02X", i-256);
				putfonts8_asc_sht(sht_back,0,16,COL8_FFFFFF,COL8_008484,s,2);
			} else if (512 <= i && i<=767) {
				if (mouse_decode(&mdec, i-512) != 0) {
					// 3字节都凑齐了，所以把它们显示出来
					sprintf(s, "[lcr %4d %4d]", mdec.x, mdec.y);
					if ((mdec.btn & 0x01) != 0) {
						s[1] = 'L';
						if (num<10){ //申请内存
						for(j=0;j<10;j++){
							if(page_list[j]==0){
								page_list[j] = buddy_alloc(buddy,4096);
								num += 1;
								break;
							}
						}
						sprintf(ss, "allocated %4d page", num);// 更新分配情况
						putfonts8_asc_sht(sht_back,0,64,COL8_FFFFFF,COL8_008484,ss,20);
						sprintf(ss, "memory %dKB free : %dKB", memtotal / (1024), buddy_status(buddy) / 1024); //更新内存情况
						putfonts8_asc_sht(sht_back,0,32,COL8_FFFFFF,COL8_008484,ss,40);
						buddy_list_status(buddy);// 更新空闲内存链表情况
						for(j=0;j<MAX_ORDER-MIN_ORDER;j++){
							lnum = buddy->free_lists_num[j];
							sprintf(ss, "%2d", lnum);
							putfonts8_asc_sht(sht_back,0+j*16,80,COL8_FFFFFF,COL8_008484,ss,8);		
						}
						sheet_refresh(sht_back, 0, 0, binfo->scrnx, 80);// 刷新文字
						}
					}
					if ((mdec.btn & 0x02) != 0) {
						s[3] = 'R';
						if(num!=0){
						for(j=0;j<10;j++){
							if(page_list[j]!=0){//回收内存
								buddy_dealloc(buddy,page_list[j],4096);
								num -= 1;
								break;
							}
						}
						sprintf(ss, "allocated %4d page", num);
						putfonts8_asc_sht(sht_back,0,64,COL8_FFFFFF,COL8_008484,ss,20);
						sprintf(ss, "memory %dKB free : %dKB", memtotal / (1024), buddy_status(buddy) / 1024);
						putfonts8_asc_sht(sht_back,0,32,COL8_FFFFFF,COL8_008484,ss,40);
						buddy_list_status(buddy);
						for(j=0;j<MAX_ORDER-MIN_ORDER;j++){
							lnum = buddy->free_lists_num[j];
							sprintf(ss, "%2d", lnum);
							putfonts8_asc_sht(sht_back,0+j*16,80,COL8_FFFFFF,COL8_008484,ss,8);		
						}
						sheet_refresh(sht_back, 0, 0, binfo->scrnx, 80);
						}
					}
					if ((mdec.btn & 0x04) != 0) {
						s[2] = 'C';
					}
					putfonts8_asc_sht(sht_back,32,16,COL8_FFFFFF,COL8_008484,s,15);
					// 移动光标
					mx += mdec.x;
					my += mdec.y;
					if (mx < 0) {
						mx = 0;
					}
					if (my < 0) {
						my = 0;
					}
					if (mx > binfo->scrnx - 1) {
						mx = binfo->scrnx - 1;
					}
					if (my > binfo->scrny - 1) {
						my = binfo->scrny - 1;
					}
					sprintf(s, "(%3d, %3d)", mx, my);
					putfonts8_asc_sht(sht_back,0,0,COL8_FFFFFF,COL8_008484,s,10);
					sheet_slide(sht_mouse, mx, my); // 包含sheet_refresh含sheet_refresh 
				}
			}
			// else if (i==10){
			// 	putfonts8_asc_sht(sht_back,0,64,COL8_FFFFFF,COL8_008484,"10[sec]",7);
			// }else if (i==3){
			// 	putfonts8_asc_sht(sht_back,0,80,COL8_FFFFFF,COL8_008484,"3[sec]",6);
			// }else if (i==1){
			// 		timer_init(timer3,&fifo,0);
			// 		boxfill8(buf_back,binfo->scrnx,COL8_FFFFFF,8,96,15,111);
			// 		timer_settime(timer3,50);
			// 		sheet_refresh(sht_back,8,96,16,112);
			// }else if (i==0){
			// 	timer_init(timer3,&fifo,1);
			// 	boxfill8(buf_back,binfo->scrnx,COL8_008484,8,96,15,111);
			// 	timer_settime(timer3,50);
			// 	sheet_refresh(sht_back,8,96,16,112);
			// }
		}
	}
}


void make_window8(unsigned char *buf, int xsize, int ysize, char *title)
{
	static char closebtn[14][16] = {
		"OOOOOOOOOOOOOOO@",
		"OQQQQQQQQQQQQQ$@",
		"OQQQQQQQQQQQQQ$@",
		"OQQQ@@QQQQ@@QQ$@",
		"OQQQQ@@QQ@@QQQ$@",
		"OQQQQQ@@@@QQQQ$@",
		"OQQQQQQ@@QQQQQ$@",
		"OQQQQQ@@@@QQQQ$@",
		"OQQQQ@@QQ@@QQQ$@",
		"OQQQ@@QQQQ@@QQ$@",
		"OQQQQQQQQQQQQQ$@",
		"OQQQQQQQQQQQQQ$@",
		"O$$$$$$$$$$$$$$@",
		"@@@@@@@@@@@@@@@@"
	};

	int x, y;
	char c;
	boxfill8(buf, xsize, COL8_C6C6C6, 0, 0, xsize - 1, 0 );
	boxfill8(buf, xsize, COL8_FFFFFF, 1, 1, xsize - 2, 1 );
	boxfill8(buf, xsize, COL8_C6C6C6, 0, 0, 0, ysize - 1);
	boxfill8(buf, xsize, COL8_FFFFFF, 1, 1, 1, ysize - 2);
	boxfill8(buf, xsize, COL8_848484, xsize - 2, 1, xsize - 2, ysize - 2);
	boxfill8(buf, xsize, COL8_000000, xsize - 1, 0, xsize - 1, ysize - 1);
	boxfill8(buf, xsize, COL8_C6C6C6, 2, 2, xsize - 3, ysize - 3);
	boxfill8(buf, xsize, COL8_000084, 3, 3, xsize - 4, 20 );
	boxfill8(buf, xsize, COL8_848484, 1, ysize - 2, xsize - 2, ysize - 2);
	boxfill8(buf, xsize, COL8_000000, 0, ysize - 1, xsize - 1, ysize - 1);
	putfonts8_asc(buf, xsize, 24, 4, COL8_FFFFFF, title);

	for (y = 0; y < 14; y++) {
		for (x = 0; x < 16; x++) {
			c = closebtn[y][x];
			if (c == '@') {
				c = COL8_000000;
			} else if (c == '$') {
				c = COL8_848484;
			} else if (c == 'Q') {
				c = COL8_C6C6C6;
			} else {
				c = COL8_FFFFFF;
			}
			buf[(5 + y) * xsize + (xsize - 21 + x)] = c;
		}
	}
	return;
}

void putfonts8_asc_sht(struct SHEET * sht,int x, int y, int c,int b,char *s, int l)
{
	boxfill8(sht->buf,sht->bxsize,b,x,y,x+l*8 -1,y+15);
	putfonts8_asc(sht->buf,sht->bxsize,x,y,c,s);
	sheet_refresh(sht,x,y,x+l*8,y+16);
	return;
}
