#include "stdafx.h"
#include "sharelister.h"
#include <dirent.h>

using namespace std;

vector<ServerInfo> servers;
ServerInfo *myServer;  //ptr into server list (our own server also resides in the share list :)

void init_server_list(){
	ServerInfo *server = new ServerInfo;
	myServer = server;
	myServer->name = string("localhost");
	myServer->address = NULL;
}

FileInfo::FileInfo(std::string path) {
  fprintf(stderr,"FileInfo: Traversing %s\n", path.c_str());
	DIR *dir = opendir(path.c_str());
	if(dir!=NULL){ // is dir
		dirent *fent; // file entry
		while((fent=readdir (dir))!=NULL) {
		  if(string(fent->d_name) != "." && string(fent->d_name) != "..") {
				file.push_back(new FileInfo(path+"/"+fent->d_name));
			}
		}
	}
	else { // is file
		fprintf(stderr,"FileInfo: File: %s\n (errval=%i)",path.c_str(), errno);
	}
}

ShareInfo::ShareInfo(std::string path) {
	fprintf(stderr,"Parsing %s\n",path.c_str());
  root = new FileInfo(path.c_str());
}

void ServerInfo::add_share(ShareInfo* share)
{
  //TODO: void add_share_to_server(uint64 UID, const struct ShareInfo &share);
  fprintf(stderr,"TODO: Adding share to server\n");
}

GtkTreeModel * WINAPI
create_and_fill_model (void)
{
  GtkTreeStore  *store;
  GtkTreeIter    iter;
	GtkTreeIter    iter2;

  store = gtk_tree_store_new (NUM_COLS, G_TYPE_STRING, G_TYPE_UINT);


  /* Append a row and fill in some data */
  gtk_tree_store_append (store, &iter, NULL);
  gtk_tree_store_set (store, &iter,
                      COL_NAME, "Heinz El-Mann",
                      COL_AGE, 51,
                      -1);

  iter2 = iter;
  /* append another row and fill in some data */
  gtk_tree_store_append (store, &iter, &iter2);
  gtk_tree_store_set (store, &iter,
                      COL_NAME, "Jane Doe",
                      COL_AGE, 23,
                      -1);

  /* ... and a third row */
  iter2 = iter;
  gtk_tree_store_append (store, &iter, &iter2);
  gtk_tree_store_set (store, &iter,
                      COL_NAME, "Joe Bungop",
                      COL_AGE, 91,
                      -1);

  return GTK_TREE_MODEL (store);
}

GtkWidget * WINAPI
create_view_and_model (GtkTreeView* aview)
{
  GtkCellRenderer     *renderer;
  GtkTreeModel        *model;
  GtkWidget           *view = (GtkWidget*)aview;

  //view = gtk_tree_view_new ();

  /* --- Column #1 --- */

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                               -1,
                                               "Name",
                                               renderer,
                                               "text", COL_NAME,
                                               NULL);

  /* --- Column #2 --- */

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                               -1,
                                               "Age",
                                               renderer,
                                               "text", COL_AGE,
                                               NULL);

  model = create_and_fill_model ();

  gtk_tree_view_set_model (GTK_TREE_VIEW (view), model);

  /* The tree view has acquired its own reference to the
   *  model, so we can drop ours. That way the model will
   *  be freed automatically when the tree view is destroyed */

  g_object_unref (model);

  return view;
}

