#pragma once

#include <wx/wx.h>

class MyApp: public wxApp
{
public:
  virtual bool OnInit();
};
class MyFrame: public wxFrame
{
public:
  MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
private:
  void OnHello(wxCommandEvent& event);
  void OnExit(wxCommandEvent& event);
  void OnAbout(wxCommandEvent& event);
  wxDECLARE_EVENT_TABLE();
};

enum {
    ID_Hello = 1
};
