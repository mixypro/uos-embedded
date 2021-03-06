UTF8 RU
NVBOOT2 - стартер флеши и загрузчик образов. предназначен для организации загрузки образа прошитого во флешу.
1)умеет загружать бинарные образы и образы ELF.
1.1) бинарные образы.
	загружаются простым копированием массива в заданную область памяти. 
	формат образа: <MARK><binary len str><bin data>
		<MARK> - 4х байтовый маркер образа. может иметь значения:
			"IMGC" - образ загружаемый в CRAM по адресу 0x98000000
			"IMGR" - образ загружаемый в xRAM по адресу 0x80000000
			"IMGr" - образ загружаемый в xRAM по адресу 0xa0000000 - не кешируемая память
		<binary len str> - текстовая строка длины образа
		<bin data>		 - собственно образ как есть
1.2) ELF образы.
	желательно перед зашивкой очищать от левой информации командой strip

2)поиск образов:	
	стартер пытается искать образ сразу за концом собственного образа. и при неудаче начинает проверять начало каждого сектора флеш с наименьшего адреса флеши.
	для микросжемы РР2 размер сектора = 64Кб, 
	для nvcom02tem - 256Кб

3) старт - собственно бинарный образ запускается после копирования как есть.
ELF образ - переходит на точку старта. 
!!! при загрузке ELFа все секции кода кладутся куда положено. поэтому важно в настройках уос: uos-conf.h указывать 
	режим UOS_START_MODE_LOADED. чтобы инициалитаро уоси не инициализировал свои сегменты данных (уже загруженные стартером) из мусора.
	Напротив для бинарных образов, важно указать UOS_START_MODE_BINARY чтобы он сам раскидал свои секции из бинарного образа.

4) использование проектом.
4.1) цели проекта эклипс.
	а) MultiCore_Configuration_Debug - отладочная версия для оценочной платы nvcom02tem размещаемая в СДРАМ.
	б) MultiCore_Configuration_Release - боевая версия для оценочной платы nvcom02tem размещаемая во флеш.
	б) flash_tst4153 				   - боевая версия для ТСТ4153 размещаемая во флеш.
4.2) действия проекта. они вызываются из меню "Make targets/build" или по Shift-F9.
	а) starter - собрать образ стартера готовый для заливки во флеш. создает файл образа boot.bin в папке построения проекта.
	б) load_boot - прошивает образ стартера в стартовую область флеш
	в) load_elf  - собирает очищенный образ целевого объекта в файл <builddir>/app.elf и прошивает образ во флеш. 
		целевого объекта - elf-образ задается переменной APP проекта (Build Variables)
		позиция во флеше, в которую кладется app.elf задается переменной IMG_ORG в файле <builddir>/target.cfg

5) общая стратегия использования стартера.
сначала надо собрать стартер и прошить его во флеш (дейтсвие load_boot). с этого момента он готов запустить образ который будет прошит во 
флеш по наименшему адресу страницы. если флеш сильно большая - этот образ будет искаться с адресов bc000000. флеш РР2А мелковата, поэтому надо 
подбирать свободные страницы за стартером. ближайшая будет bfc10000.
интересующий образ elf надо указать в переменной APP проекта. и действием load_elf его можно загружать в любое время.