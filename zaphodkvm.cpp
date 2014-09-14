/*
   zaphodkvm - software KVM switch for multiple X servers
   Copyright (C) 2014 by geekamole, released under the GNU GPLv3 or later (see COPYING)

   libsuinput used and distributed under terms of the GNU GPLv3 or later, and obtained from
      http://tjjr.fi/sw/libsuinput/

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "suinput.h"
#include "Util.h"
#include <pthread.h>
#include <string>
#include <signal.h>
#include <libudev.h>
#include <fcntl.h>
#include <map>

using namespace std;

struct ThreadInfo_t
{
   string *node_path;
   bool need_LED_refresh;
};

static uint8_t active_dest = 0;
static int kbd0, kbd1, mouse0, mouse1;
static udev *udevp = 0;
typedef map<string,ThreadInfo_t*> hw_map_t;
static hw_map_t hw_map;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static bool is_vt7 = true;
//I initialize numlock with numlockx which doesn't emit a keypress.
static bool numlock_on[] = {true, true};
static bool capslock_on[] = {false, false};

void *mouse_thread(void *arg)
{
   ThreadInfo_t &thread_info = *((ThreadInfo_t *)arg);
   string &node_path_s = *thread_info.node_path;
   char node_path[500];
   sprintf(node_path, "%s", node_path_s.c_str());
   //printf("Mouse: %s\n", node_path);
   int hwfd;
   if((hwfd = open(node_path, O_RDONLY)) != -1)
   {
      input_event ie;
      while(read(hwfd, &ie, sizeof(input_event)) == sizeof(input_event))
      {
         if(ie.type == EV_MSC)
         {
            continue; //no need for raw scancodes
         }
         if(!is_vt7)
         {
            continue; //don't send anything to VTs 1-6
         }
         //printf("Event: type %i, code %i, value: %i\n", ie.type, ie.code, ie.value);
         pthread_mutex_lock(&mutex);
         if(active_dest)
         {
            suinput_emit(mouse1, ie.type, ie.code, ie.value);
         }
         else
         {
            suinput_emit(mouse0, ie.type, ie.code, ie.value);
         }
         pthread_mutex_unlock(&mutex);
      }
   }
   //printf("Exiting mouse thread...\n");
   pthread_mutex_lock(&mutex);
   hw_map.erase(*thread_info.node_path);
   delete thread_info.node_path;
   delete &thread_info;
   pthread_mutex_unlock(&mutex);
   return NULL;
}

void *kbd_thread(void *arg)
{
   ThreadInfo_t &thread_info = *((ThreadInfo_t *)arg);
   string &node_path_s = *thread_info.node_path;
   char node_path[500];
   sprintf(node_path, "%s", node_path_s.c_str());
   bool numlock_LED = false;
   bool capslock_LED = false;
   //printf("Keyboard: %s\n", node_path);
   double last_scroll_lock_ms = 0;
   const int MAX_SCROLL_SEP_MS = 1000;
   double last_switch_ms = 0;
   const int MIN_SWITCH_SEP_MS = 500;
   int hwfd;
   if((hwfd = open(node_path, O_RDWR)) != -1)
   {
      input_event ie;
      while(read(hwfd, &ie, sizeof(input_event)) == sizeof(input_event))
      {
         //printf("Event: type %i, code %i, value: %i\n", ie.type, ie.code, ie.value);
         if(ie.type == EV_MSC)
         {
            continue; //no need for raw scancodes
         }
         if(!is_vt7)
         {
            continue; //don't send anything to VTs 1-6, and don't listen to numlock/capslock
         }
         if((ie.type == EV_KEY) && (ie.code == KEY_SCROLLLOCK) && (ie.value == 1))
         {
            double now_ms = elapsed();
            if((now_ms < last_scroll_lock_ms + MAX_SCROLL_SEP_MS) &&
               (now_ms > last_switch_ms + MIN_SWITCH_SEP_MS))
            {
               active_dest = !active_dest;
               last_switch_ms = now_ms;
            }
            last_scroll_lock_ms = now_ms;
         }
         if((ie.type == EV_KEY) && (ie.code == KEY_NUMLOCK) && (ie.value == 1))
         {
            //Try this for a while; we could get fancy as parse the output of
            //xset -q for the real lock states (poll that regularly), then refresh
            //the LED state if incorrect. But this might be sufficient.
            //This sets the status for all keyboards. Then, the individual keyboard
            //threads should change their respective LEDs (the next time they have a key hit).
            numlock_on[active_dest] = !numlock_on[active_dest];
         }

         if((numlock_LED != numlock_on[active_dest]) || thread_info.need_LED_refresh)
         {
            numlock_LED = numlock_on[active_dest];
            input_event LED_event;
            LED_event.type = EV_LED;
            LED_event.code = LED_NUML;
            LED_event.value = numlock_LED ? MSC_PULSELED : !MSC_PULSELED;
            write(hwfd, &LED_event, sizeof(input_event));
         }
         if((ie.type == EV_KEY) && (ie.code == KEY_CAPSLOCK) && (ie.value == 1))
         {
            capslock_on[active_dest] = !capslock_on[active_dest];
         }
         if((capslock_LED != capslock_on[active_dest]) || thread_info.need_LED_refresh)
         {
            capslock_LED = capslock_on[active_dest];
            input_event LED_event;
            LED_event.type = EV_LED;
            LED_event.code = LED_CAPSL;
            LED_event.value = capslock_LED ? MSC_PULSELED : !MSC_PULSELED;
            write(hwfd, &LED_event, sizeof(input_event));
         }
         thread_info.need_LED_refresh = false;

         //printf("Event: type %i, code %i, value: %i\n", ie.type, ie.code, ie.value);
         pthread_mutex_lock(&mutex);
         if(active_dest)
         {
            suinput_emit(kbd1, ie.type, ie.code, ie.value);
         }
         else
         {
            suinput_emit(kbd0, ie.type, ie.code, ie.value);
         }
         pthread_mutex_unlock(&mutex);
      }
   }

   pthread_mutex_lock(&mutex);
   hw_map.erase(*thread_info.node_path);
   delete thread_info.node_path;
   delete &thread_info;
   pthread_mutex_unlock(&mutex);
   return NULL;
}

static void finish(int)
{
   suinput_destroy(kbd0);
   suinput_destroy(kbd1);
   suinput_destroy(mouse0);
   suinput_destroy(mouse1);
   if(udevp)
   {
      udev_unref(udevp);
   }
   printf("Virtual devices deleted, exiting...\n");
   exit(0);
}

void udev_event(udev_device *dev)
{
   if(!dev)
   {
      return;
   }
   const char *node_path = udev_device_get_devnode(dev);
   const char *prop_str_usb = udev_device_get_property_value(dev, "ID_BUS");
   if(node_path && strstr(node_path, "/dev/input/event") &&
      prop_str_usb && !strcmp(prop_str_usb, "usb"))
   {
      /*printf("Got Device\n");
      printf("   Node: %s\n", udev_device_get_devnode(dev));
      printf("   Subsystem: %s\n", udev_device_get_subsystem(dev));
      printf("   Devtype: %s\n", udev_device_get_devtype(dev));*/

      const char *action = udev_device_get_action(dev);
      bool added = !action || (action && strcmp(action, "remove"));

      if(added)
      {
         pthread_mutex_lock(&mutex);
         if(!hw_map.count(node_path))
         {
            const char *prop_str_mouse = udev_device_get_property_value(dev, "ID_INPUT_MOUSE");
            if(prop_str_mouse)
            {
               pthread_t thread;
               ThreadInfo_t *arg = new ThreadInfo_t;
               arg->node_path = new string(node_path);
               pthread_create(&thread, NULL, mouse_thread, arg);
               pthread_detach(thread);
               hw_map[node_path] = arg;
            }
            const char *prop_str_kbd = udev_device_get_property_value(dev, "ID_INPUT_KEYBOARD");
            if(prop_str_kbd)
            {
               pthread_t thread;
               ThreadInfo_t *arg = new ThreadInfo_t;
               arg->node_path = new string(node_path);
               arg->need_LED_refresh = true;
               pthread_create(&thread, NULL, kbd_thread, arg);
               pthread_detach(thread);
               hw_map[node_path] = arg;
            }
         }
         pthread_mutex_unlock(&mutex);
      }
   }
}

int main(int argc, char **argv)
{
   signal(SIGINT, finish);

   struct uinput_user_dev
      user_dev_kbd0,
      user_dev_kbd1,
      user_dev_mouse0,
      user_dev_mouse1;
   memset(&user_dev_kbd0, 0, sizeof(struct uinput_user_dev));
   memset(&user_dev_kbd1, 0, sizeof(struct uinput_user_dev));
   memset(&user_dev_mouse0, 0, sizeof(struct uinput_user_dev));
   memset(&user_dev_mouse1, 0, sizeof(struct uinput_user_dev));
   sprintf(user_dev_kbd0.name, "zaphodkvm_kbd0");
   sprintf(user_dev_kbd1.name, "zaphodkvm_kbd1");
   sprintf(user_dev_mouse0.name, "zaphodkvm_mouse0");
   sprintf(user_dev_mouse1.name, "zaphodkvm_mouse1");

   kbd0 = suinput_open();
   kbd1 = suinput_open();
   mouse0 = suinput_open();
   mouse1 = suinput_open();

   //Enable every possible AT key; see /usr/include/linux/event.h
   for(int i = 1; i <= 248; ++i)
   {
      suinput_enable_event(kbd0, EV_KEY, i);
      suinput_enable_event(kbd1, EV_KEY, i);
   }
   //suinput_enable_event(kbd0, EV_MSC, MSC_SCAN);
   //suinput_enable_event(kbd1, EV_MSC, MSC_SCAN);

   int rel_axes[] = {REL_X, REL_Y, REL_WHEEL};
   for(int i = 0; i < 3; ++i)
   {
      suinput_enable_event(mouse0, EV_REL, rel_axes[i]);
      suinput_enable_event(mouse1, EV_REL, rel_axes[i]);
   }
   suinput_enable_event(mouse0, EV_KEY, BTN_LEFT);
   suinput_enable_event(mouse1, EV_KEY, BTN_LEFT);
   suinput_enable_event(mouse0, EV_KEY, BTN_RIGHT);
   suinput_enable_event(mouse1, EV_KEY, BTN_RIGHT);
   suinput_enable_event(mouse0, EV_KEY, BTN_MIDDLE);
   suinput_enable_event(mouse1, EV_KEY, BTN_MIDDLE);
   //suinput_enable_event(mouse0, EV_MSC, MSC_SCAN); //These probably aren't needed ever

   suinput_create(kbd0, &user_dev_kbd0);
   suinput_create(kbd1, &user_dev_kbd1);
   suinput_create(mouse0, &user_dev_mouse0);
   suinput_create(mouse1, &user_dev_mouse1);

   udevp = udev_new();
   if(!udevp)
   {
      printf("Can't create udev\n");
      finish(0);
   }
   udev_monitor *mon = udev_monitor_new_from_netlink(udevp, "udev");
   udev_monitor_filter_add_match_subsystem_devtype(mon, "input", NULL);
   if(udev_monitor_enable_receiving(mon))
   {
      printf("Failed to bind the udev monitor\n");
      finish(0);
   }
   int mon_fd = udev_monitor_get_fd(mon);

   udev_enumerate *enumerate = udev_enumerate_new(udevp);
   udev_enumerate_add_match_subsystem(enumerate, "input");
   udev_enumerate_scan_devices(enumerate);
   udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
   udev_list_entry *list_entry;
   udev_list_entry_foreach(list_entry, devices)
   {
      const char *path = udev_list_entry_get_name(list_entry);
      udev_device *dev = udev_device_new_from_syspath(udevp, path);
      udev_event(dev);
      udev_device_unref(dev);
   }
   udev_enumerate_unref(enumerate);

   while(1)
   {
      fd_set mon_fds;
      struct timeval tv;
      FD_ZERO(&mon_fds);
      FD_SET(mon_fd, &mon_fds);
      tv.tv_sec = 0;
      tv.tv_usec = 0;
      int ret = select(mon_fd+1, &mon_fds, NULL, NULL, &tv);

      if (ret > 0 && FD_ISSET(mon_fd, &mon_fds))
      {
         udev_device *dev = udev_monitor_receive_device(mon);
         if(dev)
         {
            udev_event(dev);
            udev_device_unref(dev);
         }
      }

      bool new_vt = false;
      FILE *vt_fp = popen("fgconsole", "r");
      if(vt_fp)
      {
         char result[100];
         if(fgets(result, sizeof(result)-1, vt_fp) != NULL)
         {
            int vt = atoi(result);
            bool old_is_vt7 = is_vt7;
            is_vt7 = (vt == 7);
            new_vt = old_is_vt7 != is_vt7;
         }
         pclose(vt_fp);
      }

      static uint8_t last_active_dest = active_dest;
      if((active_dest != last_active_dest) || new_vt)
      {
         pthread_mutex_lock(&mutex);
         hw_map_t::iterator it = hw_map.begin();
         while(it != hw_map.end())
         {
            it->second->need_LED_refresh = true;
            ++it;
         }
         pthread_mutex_unlock(&mutex);
      }
      last_active_dest = active_dest;

      Sleep(1000);
   }
   return 0;
}
