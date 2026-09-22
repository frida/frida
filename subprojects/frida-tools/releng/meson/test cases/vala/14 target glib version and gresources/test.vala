using Gtk;
using GLib;

[GtkTemplate (ui = "/org/Meson/test.ui")]
public class TestWidget : Box {
  public string text {
    get { return entry.text; }
    set { entry.text = value; }
  }

  [GtkChild]
  private unowned Entry entry;

  public TestWidget (string text) {
    this.text = text;
  }
}

void main(string[] args) {
  Gtk.init (ref args);
  var win = new Window();
  win.destroy.connect (Gtk.main_quit);

  var widget = new TestWidget ("SOME TEXT HERE");

  win.add (widget);
  win.show_all ();

  /* Exit immediately */
  Timeout.add_full (Priority.DEFAULT_IDLE, 1, () =>
    {
      Gtk.main_quit();
      return false;
    });

  Gtk.main ();
}
