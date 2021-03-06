#ifdef WEB_CHAT

#define WMCHATFILE			"/dev_hdd0/tmp/wmtmp/wmchat.htm"

#ifndef LITE_EDITION
static void webchat(char *buffer, char *templn, char *param, char *tempstr, sys_net_sockinfo_t conn_info_main)
{
	struct CellFsStat buf;

	int fd; uint64_t msiz = 0; int size = 0;

	// truncate msg log
	if(cellFsStat((char*)WMCHATFILE, &buf)!=CELL_FS_SUCCEEDED || buf.st_size>0x8000UL || buf.st_size==0)
	{
		memset(tempstr, 0, _4KB_);

		if(buf.st_size>0x8000UL)
		{
			if(cellFsOpen((char*)WMCHATFILE, CELL_FS_O_RDONLY, &fd, NULL, 0)==CELL_FS_SUCCEEDED)
			{
				cellFsLseek(fd, (buf.st_size-4080), CELL_FS_SEEK_SET, &msiz);
				cellFsRead(fd, (void *)&tempstr, 4080, &msiz);
				cellFsClose(fd);
			}
		}

		cellFsUnlink((char*)WMCHATFILE);

		if(cellFsOpen((char*)WMCHATFILE, CELL_FS_O_RDWR|CELL_FS_O_CREAT|CELL_FS_O_APPEND, &fd, NULL, 0) == CELL_OK)
		{
			strcpy(templn,	"<meta http-equiv=\"refresh\" content=\"10\">"
							"<body bgcolor=\"#101010\" text=\"#c0c0c0\">"
							"<script>window.onload=toBottom;function toBottom(){window.scrollTo(0, document.body.scrollHeight);}</script>\0");
			if(tempstr[0]) strcat(templn, "<!--");
			size = strlen(templn);
			cellFsWrite(fd, templn, size, &msiz);
			size = strlen(templn);
            if(size) cellFsWrite(fd, tempstr, size, &msiz);
		}
		cellFsClose(fd);
    }

	// append msg
	char msg[200]="", user[20]="guest\0"; char *pos;
	if(conn_info_main.remote_adr.s_addr==0x7F000001) strcpy(user,"console\0");
	if(islike(param, "/chat.ps3?"))
	{
		pos = strstr(param, "u="); if(pos) get_value(user, pos+2, 20);
		pos = strstr(param, "m="); if(pos) get_value(msg , pos+2, 200);

		sprintf(templn, "<font color=\"red%s\"><b>%s</b></font><br>%s<br><!---->", user, user, msg);

		if(cellFsOpen((char*)WMCHATFILE, CELL_FS_O_RDWR|CELL_FS_O_CREAT|CELL_FS_O_APPEND, &fd, NULL, 0) == CELL_OK)
		{
		    size = strlen(templn);
		    cellFsWrite(fd, templn, size, &msiz);
		}
		cellFsClose(fd);

		if(conn_info_main.remote_adr.s_addr!=0x7F000001) show_msg((char*)(msg));
	}

	// show msg log
	sprintf(templn, "<iframe src=\"%s\" width=\"99%%\" height=\"300\"></iframe>", WMCHATFILE); strcat(buffer, templn);

	// prompt msg
	sprintf(templn, "<hr>"
					"<form name=\"f\" action=\"\">"
					HTML_INPUT("u", "%s", "10", "5") ":" HTML_INPUT("m", "", "500", "80")
					"<input type=submit value=\"send\">"
					"</form><script>f.m.focus();</script>", user); strcat(buffer, templn);
}

#endif
#endif
