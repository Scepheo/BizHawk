struct Mouse : Controller {
  uint2 data();
  void latch(bool data);
  Mouse(bool port);
private:
  bool latched;
  unsigned counter;
  int position_x;
  int position_y;
};
