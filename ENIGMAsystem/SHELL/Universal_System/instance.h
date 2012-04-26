/********************************************************************************\
**                                                                              **
**  Copyright (C) 2008 Josh Ventura                                             **
**  Copyright (C) 2012 Alasdair Morrison                                        ** 
**                                                                              **
**  This file is a part of the ENIGMA Development Environment.                  **
**                                                                              **
**                                                                              **
**  ENIGMA is free software: you can redistribute it and/or modify it under the **
**  terms of the GNU General Public License as published by the Free Software   **
**  Foundation, version 3 of the license or any later version.                  **
**                                                                              **
**  This application and its source code is distributed AS-IS, WITHOUT ANY      **
**  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS   **
**  FOR A PARTICULAR PURPOSE. See the GNU General Public License for more       **
**  details.                                                                    **
**                                                                              **
**  You should have recieved a copy of the GNU General Public License along     **
**  with this code. If not, see <http://www.gnu.org/licenses/>                  **
**                                                                              **
**  ENIGMA is an environment designed to create games and other programs with a **
**  high-level, fully compilable language. Developers of ENIGMA or anything     **
**  associated with ENIGMA are in no way responsible for its users or           **
**  applications created by its users, or damages caused by the environment     **
**  or programs made in the environment.                                        **
**                                                                              **
\********************************************************************************/

namespace enigma {
  extern int destroycalls, createcalls;
}

int instance_create(int x,int y,int object);

namespace enigma {
  object_basic *instance_create_id(int x,int y,int object,int idg); //This is for use by the system only. Please leave be.
}

void instance_deactivate_all(bool notme);
void instance_activate_all();
void instance_activate_object(int obj);
void instance_deactivate_object(int obj);

void instance_destroy();
void instance_destroy(int id);
bool instance_exists (int obj);
int  instance_find   (int obj,int n);
int  instance_number (int obj);
int  instance_nearest (int x,int y,int obj,bool notme = false);
int  instance_furthest(int x,int y,int obj,bool notme = false);


//int instance_place(x,y,obj)
//int instance_copy(performevent)
void instance_change(int obj, bool perf);
inline void action_change_object(int obj, bool perf);





