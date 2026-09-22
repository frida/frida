#include"mainwin.h"

wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
EVT_MENU(ID_Hello, MyFrame::OnHello)
EVT_MENU(wxID_EXIT, MyFrame::OnExit)
EVT_MENU(wxID_ABOUT, MyFrame::OnAbout)
wxEND_EVENT_TABLE()

bool MyApp::OnInit() {
  MyFrame *frame = new MyFrame("Hello World", wxPoint(50, 50), wxSize(450, 340));
  frame->Show( true );
  return true;
}

MyFrame::MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
  : wxFrame(NULL, wxID_ANY, title, pos, size) {
  wxMenu *menuFile = new wxMenu;
  menuFile->Append(ID_Hello, "&Hello...\tCtrl-H",
                   "Help string shown in status bar for this menu item");
  menuFile->AppendSeparator();
  menuFile->Append(wxID_EXIT);
  wxMenu *menuHelp = new wxMenu;
  menuHelp->Append(wxID_ABOUT);
  wxMenuBar *menuBar = new wxMenuBar;
  menuBar->Append(menuFile, "&File");
  menuBar->Append(menuHelp, "&Help");
  SetMenuBar(menuBar);
  CreateStatusBar();
  SetStatusText("This is status." );
}

void MyFrame::OnExit(wxCommandEvent& event) {
  Close( true );
}

void MyFrame::OnAbout(wxCommandEvent& event) {
  //wxMessageBox("Some text", wxOK | wxICON_INFORMATION);
}

void MyFrame::OnHello(wxCommandEvent& event) {
  wxLogMessage("Some more text.");
}

#if 0
wxIMPLEMENT_APP(MyApp);
#else
// Don't open a window because this is an unit test and needs to
// run headless.
int main(int, char **) {
    wxString name("Some app");
    wxPoint p(0, 0);
    wxSize s(100, 100);
    return 0;
}

#endif
