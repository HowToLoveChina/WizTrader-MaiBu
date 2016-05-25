<?php
/**
 * 输入消息给应用端
 * @param $text 		提示的第一行
 * @param $text2		按中键后显示的第二行
 * @param $stamp 		刷新时间unixstamp
 * @param $tail  		脚下面的时间
 * @param $withnext		是否还有下一条消息
 * @output json			按json格式输出
*/
function showMaiBu($text,$text2,$stamp,$tail,$withnext=0){
	$ar['msg']=$tail;
	$ar['t2']=$text2;
	$ar['text']=$text;
	$ar['token']=$stamp;
	$ar['next']=intval($withnext);
	printf("%s",json_encode($ar);
}

?>{
	"msg":"新高难继有钱是爷",
	"t2":"<?php echo $when; ?>",
	"text":"<?php echo $str; ?>",
	"token":<?php echo $id; ?>,
	"next":<?php echo $next; ?>
}