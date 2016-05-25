#include <stdlib.h>
#include <stdio.h>
#include <string.h>
/*
2016-04-14 PM 12:00:45
相同的token没有停止存储
2016-04-14 PM 12:03:36
新数据来了没刷新
2016-04-14 PM 12:26:22
g_show_index 更新不对
2016-04-14 PM 13:04:38
UTF8汉字存储要扩大
2016-05-03 PM 20:54:56
时间计数器改成无符号，解决可能有时静态变量被覆盖问题
*/


#include "maibu_sdk.h"
#include "maibu_res.h"

#define SCREEN_ITEM_COUNT  5
#define REFRESH_COUNT_MAX	5
#define EMPTY_RECORD		"   ---"
#define NOTIFY_FLAG_CHAR '*'

//! GUI 句柄
static int8_t g_window_id;														//!主窗体的ID
static int8_t g_timer_id;														//!定时器的ID
static uint8_t g_text_id[SCREEN_ITEM_COUNT+1];									//!界面上的文本层ID
static uint8_t g_show_mode;														//!显示模式
	enum{
		E_SHOW_TEXT=0,E_SHOW_TEXT2,E_SHOW_MODE_MAX
	};
static char g_new_flag;
//多一个用于显示调试信息
static uint8_t g_title_id;													//!最上面标题行的ID
//! 资源句柄
static uint32_t g_last_request;												//!网络请求号，结束为0
//
static int16_t g_show_index;												//!当前显示的是第几条，必须是有符号的
static int16_t g_store_count;												//!当天有多少条了
static uint16_t g_timer_count;												//!计时器
static int g_refresh_time ;													//!刷新ID，决定后面的数据怎么来
	#define REFRESH_NOW g_timer_count = REFRESH_COUNT_MAX*60;
static char g_need_update;

static void global_var_int(){
	g_window_id=0;
	g_timer_id=0;
	memset(g_text_id,0,sizeof(g_text_id));
	g_title_id=0;
	g_last_request=0;
	g_show_index=0;
	g_store_count=0;
	g_timer_count=0;
	g_refresh_time=0;
	g_need_update=0;
	g_show_mode = E_SHOW_TEXT;
	g_new_flag = '-' ;
}




#define STORAGE_ID_DATA	100	/*用来存储的真实数据*/
#define STORAGE_ID_TICK	200	/*用来指示数据*/
#define NOTIFY_MAX  100
/*
组织方式
	TICK保存
	int32 计数
	int32 年月日
	USE保存
	struct storage_record * NOTIFY_MAX
*/

struct storage_tick{
	int count;		//!条数
	int tick;		//!最近的一个更新时间点
	int ymd;		//!按天更新
};

#define TEXT_SIZE 60
struct storage_record{
	int32_t  token;																	//!更新时间
	char text		[TEXT_SIZE];												//!在服务器端格式化好不再处理
	char text2		[TEXT_SIZE];												//!允许有两行交替显示
};




static int storage_get_count(void);
static uint32_t storage_get_record( int index , struct storage_record * record );
static void storage_init();
static void my_send_request();




/*
读取网络数据
	src   返回的数据
	record  要保存的结构
*/
static void get_json_data(const char * src , struct storage_record * record){
	record->text[0]=0;															//!写零用来判断是否读到
	record->text2[0]=0;															//!第二行可以没有
	maibu_get_json_int(src,"token",&record->token);								//!读时间
	maibu_get_json_str(src,"text",record->text,TEXT_SIZE);						//!读文本
	maibu_get_json_str(src,"t2",record->text2,TEXT_SIZE);						//!第二行
}



static int add_title_layer(P_Window pWin){
	LayerText  lt ;
	lt.text = "欢迎使用股神助手";
	lt.frame.origin.x = 0;
	lt.frame.origin.y = 0;
	lt.frame.size.h = 22 ;
	lt.frame.size.w = 128 ;
	lt.alignment = GAlignTopLeft ;
	lt.font_type=U_GBK_SIMSUN_16;
	lt.bound_width=0;
	Layer * layer = app_layer_create_text(&lt);
	app_layer_set_bg_color(layer, GColorBlack);
	return app_window_add_layer(pWin, layer);
}
static int add_text_layer2window(P_Window p_window,int idx){
	LayerText  lt ;
	if( idx == 0 ){
		lt.text = "　＋—————＋";
	}else
	if( idx == 1 ){
		lt.text = "　｜抓牛市好股｜";
	}else
	if( idx == 2 ){
		lt.text = "　｜躲熊市烂股｜";
	}else
	if( idx == 3 ){
		lt.text = "　｜做股市高手｜";
	}else
	if( idx == 4 ){
		lt.text = "　｜看股神助手｜";
	}else
	if( idx == 5 ){
		lt.text = "　＋—————＋";
	}else{
		lt.text = EMPTY_RECORD;
	}
	//lt.text = "零柒 600770+2.0%";
	lt.frame.origin.x = 3;
	lt.frame.origin.y = idx*16+25;
	lt.frame.size.h = 25 ;
	lt.frame.size.w = 128 - lt.frame.origin.x * 2 ;
	lt.alignment = GAlignLeft ;
	lt.font_type=U_GBK_SIMSUN_14;
	lt.bound_width=0;
	Layer * layer = app_layer_create_text(&lt);
	return app_window_add_layer(p_window, layer);
}

static void debug_show_to_text_id(char * msg , uint8_t layer_id){
	P_Window p_window = (P_Window)app_window_stack_get_window_by_id(g_window_id);	
	if (NULL == p_window){
		return;
	}
	Layer * layer = app_window_get_layer_by_id(p_window,layer_id);
	app_layer_set_text_text(layer,msg);
	g_need_update = 1;
}

#define DEBUG_TEXT(msg)		debug_show(msg,SCREEN_ITEM_COUNT+1)

static void debug_show(char * msg,uint8_t idx){
	if( idx == 0 ){
		debug_show_to_text_id(msg,g_title_id);
		return;
	}
	debug_show_to_text_id(msg,g_text_id[idx-1]);
}
/*
刷新屏幕
*/
void my_refresh_screen(void){
	struct storage_record record;												//!存储记录
	char i ;																	//!临时变量
	Layer * layer;
	P_Window p_window = (P_Window)app_window_stack_get_window_by_id(g_window_id);	
	if (NULL == p_window){
		return;
	}
	int m = storage_get_count();												//!取得最大计数
	for (i=0;i< SCREEN_ITEM_COUNT ; i ++ ){										//!每条都更新
		layer = app_window_get_layer_by_id(p_window,g_text_id[i]);				//!取得文本层
		if( g_show_index >= m ){												//!超过最后的只显示***
			app_layer_set_text_text(layer,EMPTY_RECORD);
		}else{
			storage_get_record(g_show_index+i,&record);
			if( g_show_mode == E_SHOW_TEXT ){
				app_layer_set_text_text(layer,record.text);							//!可以显示的就加上去
			}else
			if( g_show_mode == E_SHOW_TEXT2 ){
				app_layer_set_text_text(layer,record.text2);							//!可以显示的就加上去
			}
		}
	}
	g_need_update = 1;
}




/* 选择键 */
static void my_key_select(void *context){
	//! 此处可以显示交易菜单
	g_show_mode ++ ; 
	g_show_mode %= E_SHOW_MODE_MAX;
	//! 立即刷新
	REFRESH_NOW
}
/* 返回键 */
static void my_key_back(void *context){
	//! 退出应用？
	P_Window p_window = (P_Window)context;
	if (NULL == p_window){
		return ;
	}
	app_window_stack_pop(p_window);
	app_service_timer_unsubscribe(g_timer_id);
	global_var_int();
}
/* 上键 */
static void my_key_up(void *context){
	g_show_index -= SCREEN_ITEM_COUNT;											//!上翻减
	if( g_show_index < 0 ){														//!不小于零
		g_show_index = 0 ;
	}
	my_refresh_screen();
}
/*
读取一条显示记录
*/
static uint32_t storage_get_record( int index , struct storage_record * record ){
	int n = storage_get_count();
	memset(record,0,sizeof(struct storage_record));
	if( index >= n ){
		strcpy(record->text,EMPTY_RECORD);
		return ;
	}
	int offset = index * sizeof(struct storage_record) + sizeof(struct storage_tick);
	return app_persist_read_data(STORAGE_ID_DATA,offset,(void*)record,sizeof(struct storage_record));
}
/*
读取当天已经有多少提醒了
*/
static int storage_get_count(void){
	struct storage_tick  tick;
	if( app_persist_read_data(STORAGE_ID_TICK,0,(void*)&tick,sizeof(struct storage_tick)) == 0 ){;//!读出来
		storage_init();
		return 0;
	}	
	return tick.count;
}
/*
追加数据
*/
static void storage_save_record(struct storage_record * record){
	struct storage_tick  tick;
	app_persist_read_data(STORAGE_ID_TICK,0,(void*)&tick,sizeof(struct storage_tick));			//!读出来
	if( record->token <= tick.tick ){
		return;
	}
	g_store_count = ++tick.count;												//!增加
	//! 反复刷，测试翻页
	tick.tick = record->token;
	app_persist_write_data_extend(STORAGE_ID_TICK,(void*)&tick,sizeof(struct storage_tick));	//!重新写入
	//!写进去
	app_persist_write_data(STORAGE_ID_DATA,(void*)record,sizeof(struct storage_record));
	//! 
	//! 在刷新范围内就刷新
	if( SCREEN_ITEM_COUNT + g_show_index >=  g_store_count ){
		my_refresh_screen();
	}
	maibu_service_vibes_pulse(VibesPulseTypeShort,0);
	g_new_flag = NOTIFY_FLAG_CHAR ;//有更新的标志
	
}
/*
把计数器清空
*/
static void storage_day_init(){
	struct storage_tick  tick;
	struct date_time now;
	//! 读取当前时间,比较一下是否初始化过了
	app_service_get_datetime(&now);
	app_persist_read_data(STORAGE_ID_TICK,0,(void*)&tick,sizeof(struct storage_tick));			//!读出来
	if( tick.ymd == (now.year*10000 + now.mon*100 + now.mday) ){
		g_store_count = tick.count ;
		return;
	}
	//
	g_store_count = 0 ;
	//
	tick.tick = 0 ;
	tick.count=0;
	tick.ymd = (now.year*10000 + now.mon*100 + now.mday);
	app_persist_create(STORAGE_ID_DATA,4096);								//!外部存储是512K的页，所以用全了
	app_persist_create(STORAGE_ID_TICK,4096);								//!外部存储是512K的页，所以用全了
	app_persist_write_data_extend(STORAGE_ID_TICK, (void*)&tick, sizeof(tick) );			//!把计数器清空
	app_persist_write_data_extend(STORAGE_ID_DATA, (void*)&tick, sizeof(tick) );			//!把计数器清空
}

static void storage_init(){
	struct storage_tick  tick;
	struct date_time now;
	//! 读取当前时间,比较一下是否初始化过了
	app_service_get_datetime(&now);
	//
	g_store_count = 0 ;
	tick.count=0;
	tick.tick = 0 ;
	tick.ymd = (now.year*10000 + now.mon*100 + now.mday);
	app_persist_create(STORAGE_ID_DATA,4096);								//!外部存储是512K的页，所以用全了
	app_persist_create(STORAGE_ID_TICK,4096);								//!外部存储是512K的页，所以用全了
	app_persist_write_data_extend(STORAGE_ID_TICK, (void*)&tick, sizeof(tick) );//!把计数器清空
	app_persist_write_data_extend(STORAGE_ID_DATA, (void*)&tick, sizeof(tick) );//!把计数器清空
}


/* 下键 */
static void my_key_down(void *context){
	if( (g_show_index + SCREEN_ITEM_COUNT) >= g_store_count ){					//!保证不过页尾
		g_show_index = g_store_count-g_store_count%SCREEN_ITEM_COUNT;
		g_new_flag = ' ';
	}else{
		g_show_index += SCREEN_ITEM_COUNT;										//!页号加上
	}
	my_refresh_screen();														//!刷屏
}





//! 界面初始化
static P_Window my_window_create(void){
	P_Window p_window = app_window_create(); 
	if (p_window == NULL){
		return NULL;
	}
	//! 初始化布局
	int i = 0 ; 
	for(i=0;i<SCREEN_ITEM_COUNT+1;i++){
		g_text_id[i] = add_text_layer2window(p_window,i);						//! 内容条
	}
	g_title_id = add_title_layer(p_window);										//! 顶部条
	//! 按键回调
	app_window_click_subscribe(p_window, ButtonIdDown, 	my_key_down);
	app_window_click_subscribe(p_window, ButtonIdUp, 	my_key_up);
	app_window_click_subscribe(p_window, ButtonIdSelect,my_key_select);
	app_window_click_subscribe(p_window, ButtonIdBack,	my_key_back);
	return p_window;
}
static int get_stock_record(const char *  buff, struct storage_record * record){
	int32_t t=0;
	get_json_data(buff,record);
	//! 是否成功
	if( record->text[0] == 0 || record->token == 0 ){
		return -1;
	}
	g_refresh_time = record->token ; 
	//! 还有下一条么？
	maibu_get_json_int(buff,"next",&t);	
	//有下条就让他再刷新
	if( t > 0 ){
		//DEBUG_TEXT("继续更新");
		my_send_request();
	}else{
		//DEBUG_TEXT("普通更新");
	}
	/*	2016-04-14 AM 11:18:45  成功
	char text[20];
	debug_show(record->text,1);
	sprintf(text,"%d",record->token);
	debug_show(text,2);
	sprintf(text,"%d",t);
	debug_show(text,3);
	*/
	return 0;
}


static void my_web_callback( const uint8_t *buff,  uint16_t size ){
	//! 取得总数
	int count;
	struct storage_record  record;
	memset(&record,0,sizeof(record));
	if( get_stock_record(buff,&record) < 0 ){
		DEBUG_TEXT("取结果失败");
		return;
	}else{
		//DEBUG_TEXT("取结果成功");
		/* 读取了一条新记录存起来 */
		storage_save_record(&record);
	}
	//!底部消息
	record.text[0]=0;
	maibu_get_json_str(buff,"msg",record.text,TEXT_SIZE);						//!读文本
	if( record.text[0]!=0 ){
		DEBUG_TEXT(record.text);
	}

}
/*
通讯回调，不论成功失败置0，表示可以发起下一次请求
*/
static void my_result_callback(enum ECommResult result, uint32_t comm_id, void *context){
	if( g_last_request == comm_id ){
		/*
		if( result == ECommResultSuccess ){
			DEBUG_TEXT("网络通讯完成");
		}else{
			DEBUG_TEXT("网络通讯失败");
		}
		*/
		//! 完成了
		g_last_request = 0 ;
	}
	
}
/*
测试是不是在交易时间
*/
static int is_market_time(void){
	return 0;
	struct date_time now;
	app_service_get_datetime(&now);
	if( now.wday>5){
		//debug_show("非工作日",0);
		return -1;
	}
	if( now.hour < 9 || now.hour == 12 || now.hour > 15 ){
		//debug_show("小于9时",0);
		return -1;
	}
	if( now.hour == 9 && now.min < 15 ){
		g_show_index=0;//!从第一个开始显示
		//debug_show("马上开盘",0);
		return -1;
	}
	if( now.hour == 11 && now.min > 30 ){
		//debug_show("午休呢",0);
		return -1;
	}
	//debug_show("交易时间",0);
	return 0;
}
/*
发送通知到桌面
*/
void  notify_desktop(){
}
/*
发送网络请求，一次只能取一条
*/
static void my_send_request(){
	int8_t result;
	char account[20];
	char osver[20];
	char watchid[20];
	char url[100];
	//! 取得基本的信息发送上去
	
	result = maibu_get_user_account(account,20);
	if( result <= 0){
		strcpy(account,"guest");
		//return;
	}
	account[result]=0;//tomac
	
	result = maibu_get_os_version(osver,20);
	if( result <= 0){
		return;
	}
	osver[result]=0;

	result = maibu_get_watch_id (watchid,20);
	if( result <= 0){
		return;
	}
	watchid[result]=0;

	sprintf(url,"http://wiztrader.chinacloudapp.cn/maibu/?w=%s&o=%s&u=%s&t=%d",watchid,osver,account,g_refresh_time);
	//! 保存请求的ID
	//DEBUG_TEXT("发送请求");
	g_last_request = maibu_comm_request_web(url,"text,token,next,t2,msg",0);
}

static void my_update_time(void){
	char line[30];
	/* 根据窗口ID获取窗口句柄，把时间刷新了 */
	P_Window p_window = (P_Window)app_window_stack_get_window_by_id(g_window_id);	
	if (NULL == p_window){
		return;
	}
	Layer * layer = app_window_get_layer_by_id(p_window,g_title_id);
	if( layer == NULL ){
		return;
	}
	struct date_time now;
	app_service_get_datetime(&now);
	sprintf(line,"%02d/%02d%c   %02d:%02d:%02d  ",(g_show_index+1),g_store_count,g_new_flag,now.hour,now.min,now.sec);
	//sprintf(line,"%02d/%02d    %02d:%02d:%02d",(g_show_index),g_store_count,now.hour,now.min,now.sec);
	app_layer_set_text_text(layer,line);
	g_need_update=1;
}


/* 检测刷新时间 */
static void my_timer_callback(date_time_t tick_time, uint32_t millis, void *context){
	my_update_time();
	g_timer_count ++ ;
	if( is_market_time( ) < 0 ){												//!非交易时间
		if( g_timer_count < REFRESH_COUNT_MAX*60){								//!非交易时间刷新率减少
			goto my_timer_callback_exit;
		}
	}
	if( g_timer_count < REFRESH_COUNT_MAX ){									//!未达到刷新时间
		//sprintf(line,"刷新在%d秒后",REFRESH_COUNT_MAX-g_timer_count);
		//debug_show(line,0);
		goto my_timer_callback_exit;
	}
	g_timer_count = 0 ;															//!到达后清零
	my_refresh_screen();
	storage_day_init();
	if( g_new_flag == NOTIFY_FLAG_CHAR ){
		/* 通知有记录来了 */
		notify_desktop(1);
	}
	//!上次通讯完成了，这次再做
	if( g_last_request == 0 ){
		my_send_request();			//这里发送请求
	}
my_timer_callback_exit:
	if( g_need_update != 0 ){		//按键后肯定会把g_need_update置1
		P_Window p_window = (P_Window)app_window_stack_get_window_by_id(g_window_id);	
		if (NULL != p_window){
			app_window_update(p_window);
		}
		g_need_update = 0 ;
	}
}
int main()
{
	global_var_int();
	//! 测试用
	//storage_init();
	/* 创建窗口 */
	P_Window p_window = my_window_create();
	/* 放入窗口栈显示 */
	g_window_id = app_window_stack_push(p_window);
	//
	maibu_comm_register_web_callback(my_web_callback);							//!注册回调
	maibu_comm_register_result_callback(my_result_callback);					//!注册结果回调
	/* 启动立即刷新 */
	g_timer_count = REFRESH_COUNT_MAX-2 ;
	g_store_count = storage_get_count();
	g_last_request= 0;
	/*添加定时器*/
	g_timer_id = app_service_timer_subscribe(1000, my_timer_callback, (void*)p_window);	
	//
	return 0;

}

