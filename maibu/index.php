<?php
/**
 * ������Ϣ��Ӧ�ö�
 * @param $text 		��ʾ�ĵ�һ��
 * @param $text2		���м�����ʾ�ĵڶ���
 * @param $stamp 		ˢ��ʱ��unixstamp
 * @param $tail  		�������ʱ��
 * @param $withnext		�Ƿ�����һ����Ϣ
 * @output json			��json��ʽ���
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
	"msg":"�¸��Ѽ���Ǯ��ү",
	"t2":"<?php echo $when; ?>",
	"text":"<?php echo $str; ?>",
	"token":<?php echo $id; ?>,
	"next":<?php echo $next; ?>
}