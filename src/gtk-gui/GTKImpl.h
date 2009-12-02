#ifndef GMPMPC_H
	#define GMPMPC_H
	#include "../UFTTCore.h"
	#include "../UFTTSettings.h"
	#include "dispatcher_marshaller.h"
	#include <iostream>
	#include <gtkmm/box.h>
	#include <gtkmm/main.h>
	#include <gtkmm/menu.h>
	#include <gtkmm/paned.h>
	#include <gtkmm/frame.h>
	#include <gtkmm/window.h>
	#include <gtkmm/treeview.h>
	#include <gtkmm/textview.h>
	#include <gtkmm/uimanager.h>
	#include <gtkmm/scrolledwindow.h>
	#include <gtkmm/radiobuttongroup.h>

	class UFTTWindow : public Gtk::Window {
		public:
			UFTTWindow(UFTTCore& _core, UFTTSettings& _settings);

		private:
				UFTTCore& core;
				UFTTSettings& settings;
			/* Variables */
//			DispatcherMarshaller dispatcher; // Execute a function in the gui thread

			/* Helpers */
			Glib::RefPtr<Gtk::UIManager>   m_refUIManager;
			Glib::RefPtr<Gtk::ActionGroup> m_refActionGroup;

			/* Containers (widgets not referenced by code) */

			/* Widgets (referenced by code) */
			Gtk::Menu* menubar_ptr; // Menu is created dynamically using UIManager
			Gtk::RadioButtonGroup menu_options_check_updates_frequency_radio_button_group;
			Gtk::VBox             menu_main_paned_vbox;
			Gtk::HPaned           main_paned;
			Gtk::VPaned           share_task_list_vpaned;
			Gtk::Frame            share_list_frame;
			Gtk::Frame            task_list_frame;
			Gtk::Frame            debug_log_frame;
			Gtk::TextView         debug_log_textview;
			Gtk::TreeView         share_list_treeview;
			Gtk::TreeView         task_list_treeview;

			/* Functions */
			void construct_gui();
			void on_menu_file_quit();
	};

#endif