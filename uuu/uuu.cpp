/*
* Copyright 2018 NXP.
*
* Redistribution and use in source and binary forms, with or without modification,
* are permitted provided that the following conditions are met:
*
* Redistributions of source code must retain the above copyright notice, this
* list of conditions and the following disclaimer.
*
* Redistributions in binary form must reproduce the above copyright notice, this
* list of conditions and the following disclaimer in the documentation and/or
* other materials provided with the distribution.
*
* Neither the name of the NXP Semiconductor nor the names of its
* contributors may be used to endorse or promote products derived from this
* software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
*/

#include <iostream>
#include <stdio.h>
#include <thread>
#include <atomic>
#include <iomanip>
#include <map>
#include <mutex>
#include <vector>
#include <sstream>

#include "../libuuu/libuuu.h"

char g_sample_cmd_list[] = {
#include "uuu.clst"
};

using namespace std;

void print_help()
{
	printf("uuu [-d -v] u-boot.imx\\flash.bin\n");
	printf("\tDownload u-boot.imx\\flash.bin to board by usb\n");
	printf("\t -d      Deamon mode, wait for forever.\n");
	printf("\t         Start download once detect known device attached\n");
	printf("\t -v      Print build in protocal config informaiton");
	printf("\n");
	printf("uuu [-d -v] cmdlist\n");
	printf("\tRun all commands in file cmdlist\n");
	printf("\n");
	printf("uuu [-d -v] SDPS: boot flash.bin\n");
	printf("\tRun command SPDS: boot flash.bin\n");
	printf("\n");

	size_t start = 0, pos = 0;
	string str= g_sample_cmd_list;
	
	bool bprint = false;
	while ((pos = str.find('\n',pos)) != str.npos)
	{
		string s = str.substr(start, pos - start);
		if (s.substr(0, 6) == "# ----")
			bprint = true;

		if (bprint)
		{
			if (s[0] == '#')
			{
				printf(&(s[1]));
				printf("\n");
			}
		}
		pos += 1;
		start = pos;
	}
}
void print_version()
{
	printf("uuu (universal update utitle) for nxp imx chips -- %s\n\n", uuu_get_version_string());
}

int print_cfg(const char *pro, const char * chip, const char *compatible, uint16_t pid, uint16_t vid, uint16_t bcdVersion, void *p)
{
	if (bcdVersion == 0xFFFF)
		printf("\t%s\t %s\t 0x%04x\t 0x%04x\n", pro, chip, pid, vid);
	else
		printf("\t%s\t %s\t 0x%04x\t 0x%04x\t 0x%04x\n", pro, chip, pid, vid, bcdVersion);
	return 0;
}

int polling_usb(std::atomic<int>& bexit);

int g_overall_status;
int g_overall_okay;
int g_overall_failure;

class ShowNotify
{
public:
	string m_cmd;
	string m_dev;
	int	m_trans_size;
	int m_trans_pos;
	int m_status;
	int m_cmd_total;
	int m_cmd_index;
	string m_last_err;
	int m_done;
	
	ShowNotify()
	{
		m_trans_size = m_trans_pos = 0;
		m_status = 0;
		m_cmd_total = 0;
		m_cmd_index = 0;
		m_done = 0;
	}

	bool update(uuu_notify nt)
	{
		if (nt.type == uuu_notify::NOFITY_DEV_ATTACH)
		{
			m_dev = nt.str;
			m_done = 0;
			m_status = 0;
		}
		if (nt.type == uuu_notify::NOTIFY_CMD_START)
		{
			m_cmd = nt.str;
		}
		if (nt.type == uuu_notify::NOTIFY_TRANS_SIZE)
		{
			m_trans_size = nt.total;
			return false;
		}
		if (nt.type == uuu_notify::NOTIFY_CMD_TOTAL)
		{
			m_cmd_total = nt.total;
			return false;
		}
		if (nt.type == uuu_notify::NOTIFY_CMD_INDEX)
		{
			m_cmd_index = nt.index;
			return false;
		}
		if (nt.type == uuu_notify::NOTIFY_DONE)
		{
			if (m_status)
				g_overall_failure++;
			else
				g_overall_okay++;

			m_done = 1;
		}
		if (nt.type == uuu_notify::NOTIFY_CMD_END)
		{
			if(nt.status)
			{
				g_overall_status = nt.status;
				m_last_err = uuu_get_last_err_string();
			}
			m_status |= nt.status;
			if (m_status)
				g_overall_failure++;
		}
		if (nt.type == uuu_notify::NOTIFY_TRANS_POS)
		{
			if (m_trans_size == 0)
				return false;

			if ((nt.index - m_trans_pos) < (m_trans_size / 100))
				return false;

			m_trans_pos = nt.index;
		}
		return true;
	}

	void print()
	{
		cout << m_dev.c_str() << std::setw(10- m_dev.size());
		
		cout << m_cmd_index + 1 << "/" << m_cmd_total << std::setw(2);

		cout<< "[";
		int width=40;
		int s = 0;

		if (m_status)
		{
			cout << m_last_err.c_str() << endl;
			return;
		}

		if (m_done)
		{
			cout << "Done" << std::setw(width - 3);
		}else
		{
			for (int i = 1; i <= width; i++)
			{
				if (m_trans_size == 0)
				{
					cout << " ";
					continue;
				}

				if (m_trans_pos*width / (m_trans_size*i))
				{
					cout << "=";
					s = 1;
				}
				else
				{
					if (s)
					{
						s = 0;
						cout << ">";
					}
					else
					{
						cout << " ";
					}
				}
			}
		}
		if(m_cmd.length() > 40)
			cout << "] " << m_cmd.substr(0, 40).c_str() << "...." << endl;
		else
			cout << "] " << m_cmd.c_str() << endl;
	}
};

static map<string, ShowNotify> g_map_path_nt;
mutex g_callback_mutex;

int progress(uuu_notify nt, void *p)
{
	map<uint64_t, ShowNotify> *np = (map<uint64_t, ShowNotify>*)p;
	map<string, ShowNotify>::iterator it;
	
	std::lock_guard<std::mutex> lock(g_callback_mutex);

	if ((*np)[nt.id].update(nt))
	{
		if(!(*np)[nt.id].m_dev.empty())
			g_map_path_nt[(*np)[nt.id].m_dev] = (*np)[nt.id];

		cout << "Succues:" << g_overall_okay << "\tFailure:" << g_overall_failure <<"\t\tWait for Known USB Device Appear..."<<endl << endl;;

		for (it = g_map_path_nt.begin(); it != g_map_path_nt.end(); it++)
			it->second.print();

		for (int i=0;i<g_map_path_nt.size()+2; i++)
			cout <<"\x1B[1F";
	}
	if (nt.type == uuu_notify::NOTIFY_THREAD_EXIT)
		np->erase(nt.id);

	return 0;
}
#ifdef _MSC_VER

#define DEFINE_CONSOLEV2_PROPERTIES
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

bool enable_vt_mode()
{
	// Set output mode to handle virtual terminal sequences
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	DWORD dwMode = 0;
	if (!GetConsoleMode(hOut, &dwMode))
	{
		return false;
	}

	dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	if (!SetConsoleMode(hOut, dwMode))
	{
		return false;
	}
	return true;
}
#else
bool enable_vt_mode() {}
#endif

int main(int argc, char **argv)
{
	print_version();

	enable_vt_mode();

	if (argc == 1)
		print_help();
	
	int deamon = 0;
	int step = 0;
	string filename; 
	string cmd;
	int verbose = 0;

	for (int i = 1; i < argc; i++)
	{
		string s = argv[i];
		if (!s.empty() && s[0] == '-')
		{
			if (s == "-d")
			{
				deamon = 1;
			}else if (s == "-s")
			{
				step = 1;
			}
			else if (s == "-v")
			{
				verbose = 1;

			}
			else if (s == "-h")
			{
				print_help();
				return 0;
			}else
			{
				cout << "Unknown option: " << s.c_str();
				return -1;
			}
		}else if (!s.empty() && s[s.size() - 1] == ':')
		{
			for (int j = i; j < argc; j++)
			{
				s = argv[j];
				cmd.append(s);
				cmd.append(" ");
			}
			break;
		}
		else
		{
			filename = s;
			break;
		}
	}

	if (deamon && step)
	{
		printf("Error: -d -s Can't apply at the same time\n");
		return -1;
	}

	if (verbose)
	{
		printf("Build in config:\n");
		printf("\tPctl\tChip\tPid\tVid\t\BcdVersion\n");
		printf("\t==========================================\n");
		uuu_for_each_cfg(print_cfg, NULL);
		printf("\n");
	}

	map<uint64_t, ShowNotify> nt_session;

	uuu_register_notify_callback(progress, &nt_session);

	if (!cmd.empty())
	{
		int ret;
		ret = uuu_run_cmd(cmd.c_str());

		for (int i = 0; i < g_map_path_nt.size()+3; i++)
			printf("\n");
		if(ret)
			printf("\nError: %s\n", uuu_get_last_err_string());
		else
			printf("Okay");

		return ret;
	}

	int ret = uuu_auto_detect_file(filename.c_str());
	if (ret)
	{
		cout << "Error:" << uuu_get_last_err_string();
		return ret;
	}
	uuu_wait_uuu_finish(deamon);

	return g_overall_status;
}
