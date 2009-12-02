#ifndef SHARE_LIST_H
	#define SHARE_LIST_H
	#include "../UFTTCore.h"
	#include "dispatcher_marshaller.h"
	#include <gtkmm/box.h>
	#include <gtkmm/treeview.h>
	#include <gtkmm/alignment.h>
	#include <gtkmm/liststore.h>
	#include <gtkmm/scrolledwindow.h>
	#include <gtkmm/filechooserbutton.h>

	class ShareList : public Gtk::VBox {
		public:
			ShareList(UFTTSettingsRef _settings, Gtk::Window& _parent_window);
			void clear();
			void download_selected_shares();
			void set_backend(UFTTCoreRef _core);
		private:
			UFTTCoreRef core;
			UFTTSettingsRef settings;
			Gtk::Window& parent_window;
			DispatcherMarshaller dispatcher; // Execute a function in the gui thread
			class ShareListColumns : public Gtk::TreeModelColumnRecord {
				public:
					ShareListColumns() {
						add(user_name);
						add(share_name);
						add(host_name);
						add(protocol);
						add(url);
						add(share_id);
					}

					Gtk::TreeModelColumn<Glib::ustring> user_name;
					Gtk::TreeModelColumn<Glib::ustring> share_name;
					Gtk::TreeModelColumn<Glib::ustring> host_name;
					Gtk::TreeModelColumn<Glib::ustring> protocol;
					Gtk::TreeModelColumn<Glib::ustring> url;
					Gtk::TreeModelColumn<ShareID>       share_id;
			};
			ShareListColumns              share_list_columns;
			Glib::RefPtr<Gtk::ListStore>  share_list_liststore;
			Gtk::TreeView                 share_list_treeview;
			Gtk::ScrolledWindow           share_list_scrolledwindow;
			Gtk::Alignment                download_destination_path_alignment;
			Gtk::HBox                     download_destination_path_hbox;
			Gtk::VBox                     download_destination_path_vbox;
			Gtk::Entry                    download_destination_path_entry;
			Gtk::Label                    download_destination_path_label;
			Gtk::FileChooserButton        browse_for_download_destination_path_button;

			// Functions / callbacks
			sigc::connection on_download_destination_path_entry_signal_changed_connection;
			void on_download_destination_path_entry_signal_changed();
			void on_browse_for_download_destination_path_button_signal_current_folder_changed();
			void on_share_list_treeview_signal_drag_data_received(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, const Gtk::SelectionData& selection_data, guint info, guint time);
			void on_signal_add_share(const ShareInfo& info);
	};

#endif // SHARE_LIST_H