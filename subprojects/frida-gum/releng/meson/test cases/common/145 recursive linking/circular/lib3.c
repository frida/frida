int get_st1_prop (void);
int get_st2_prop (void);

int get_st3_value (void) {
  return get_st1_prop () + get_st2_prop ();
}
