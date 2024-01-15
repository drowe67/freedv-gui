# Set of tests to ignore. On some platforms, libsamplerate's tests somehow get included
# with our own, so it causes problems with e.g. CI builds to keep these.
set(CTEST_CUSTOM_TESTS_IGNORE
  misc_test
  termination_test
  callback_hang_test
  downsample_test
  simple_test
  callback_test
  reset_test
  clone_test
  nullptr_test
  multi_channel_test
  varispeed_test
  float_short_test
  snr_bw_test)