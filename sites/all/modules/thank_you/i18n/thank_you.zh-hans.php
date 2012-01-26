<?php

$TYmsgs['zh-hans'] = array(
	"thank_you_from_name" => "Sue Gardner",
	"thank_you_to_name" => "{contact.display_name}",
	"thank_you_to_name_secondary" => "维基媒体基金会的朋友",
	"thank_you_subject" => "来自维基媒体基金会的感谢信",
	"thank_you_unsubscribe_title" => "维基媒体基金会退订",
	"thank_you_unsubscribe_button" => "取消订阅",
	"thank_you_unsubscribe_confirm" => "您已经成功从我们的邮件列表中移除",
//	"thank_you_unsubscribe_warning" => "",
//	"thank_you_unsubscribe_success" => "",
	"thank_you_unsubscribe_delay" => "请静候此更改生效，可能需要四（4）天时间。我们对于在此期间您收到的任何电邮表示歉意。如果您还有任何问题，请联系 <a href='mailto:donations@wikimedia.org'>donations@wikimedia.org</a>",
	"thank_you_unsubscribe_fail" => "在处理您的请求时发生错误，请联系 <a href='mailto:donations@wikimedia.org'>donations@wikimedia.org</a> 。",
);
$TYmsgs['zh-hans']['thank_you_body_plaintext'] =<<<'EOD'
尊敬的{contact.first_name}
您实在太棒了，十分感谢您为维基媒体基金会捐款！
这正是我们得以继续存在的方式——正如同您一样的人们，捐出五美元、二十美元、一百美元。去年让我印象最深刻的，是来自一名英国小姑娘的五英镑捐款，她劝说自己的父母允许她捐出零用钱。正是像您一样的人们，同那位小姑娘一起，让维基百科继续为全世界的人们免费且便捷地提供无偏见的信息成为可能。我向所有帮助支持它的人们，以及那些有心却无力支持的人们，道声感激不尽。
我知道，想要无视我们的呼吁很简单，而我很高兴您并没有这么做。我谨以我个人，以及数以万计的维基百科编撰志愿者：感谢您协助我们，让世界变得更美好。我们会谨慎使用您的捐款，并感谢您对我们的信赖。
感谢，
Sue Gardner
维基媒体基金会执行主席

---

您的捐赠信息：您于{contribution.date}进行捐款，总计{contribution.source}。
这封信件可以作为您捐款的证明。这次捐助行为没有提供任何实物或服务，无论是全部还是部分。维基媒体基金会是一家受美国税法501(c)(3)条款免税的非营利性慈善组织。我们的办公地址是：149 New Montgomery, 3rd Floor, San Francisco, CA, 94105。美国免税查询电话：20-0049703

---

免打扰选项：
作为捐赠者，我们希望能够持续告知您我们的社群活动和筹款信息。如果您不想收到来自我们的这些邮件，请点击下方链接，我们将把您从邮件名单中移除：
{unsubscribe_link}
EOD;

$TYmsgs['zh-hans']['thank_you_body_html'] =<<<'EOD'
<p>尊敬的{contact.first_name}</p>

<p>您实在太棒了，十分感谢您为维基媒体基金会捐款！</p>

<p>这正是我们得以继续存在的方式——正如同您一样的人们，捐出五美元、二十美元、一百美元。去年让我印象最深刻的，是来自一名英国小姑娘的五英镑捐款，她劝说自己的父母允许她捐出零用钱。正是像您一样的人们，同那位小姑娘一起，让维基百科继续为全世界的人们免费且便捷地提供无偏见的信息成为可能。我向所有帮助支持它的人们，以及那些有心却无力支持的人们，道声感激不尽。</p>

<p>我知道，想要无视我们的呼吁很简单，而我很高兴您并没有这么做。我谨以我个人，以及数以万计的维基百科编撰志愿者：感谢您协助我们，让世界变得更美好。我们会谨慎使用您的捐款，并感谢您对我们的信赖。</p>

<p>感谢<br />
Sue Gardner<br />
<b>维基媒体基金会执行主席</b><br />
</p>

<p>您的捐赠信息：您于{contribution.date}进行捐款，总计{contribution.source}。</p>

<p>这封信件可以作为您捐款的证明。这次捐助行为没有提供任何实物或服务，无论是全部还是部分。维基媒体基金会是一家受美国税法501(c)(3)条款免税的非营利性慈善组织。我们的办公地址是：149 New Montgomery, 3rd Floor, San Francisco, CA, 94105。美国免税查询电话：20-0049703</p>

<div style="padding:0 10px 5px 10px; border:1px solid black;">
<p><i>免打扰选项：</i></p>
<p>作为捐赠者，我们希望能够持续告知您我们的社群活动和筹款信息。如果您不想收到来自我们的这些邮件，请点击下方链接，我们将把您从邮件名单中移除：</p>
<a style="padding-left: 25px;" href="{unsubscribe_link}">取消订阅</a>
</div>
EOD;

