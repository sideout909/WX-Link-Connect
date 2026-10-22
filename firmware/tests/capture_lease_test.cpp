#include "capture_lease.h"

#include <cassert>
#include <cstdio>

using wxconnect::capture::CaptureLease;

int main() {
  CaptureLease browser;
  assert(browser.begin(CaptureLease::Owner::Browser, 1000));
  assert(!browser.begin(CaptureLease::Owner::Usb, 1001));
  assert(!browser.expired(21000));
  assert(browser.heartbeat(CaptureLease::Owner::Browser, 20000));
  assert(!browser.expired(40000));
  assert(browser.expired(40001));
  assert(!browser.heartbeat(CaptureLease::Owner::Usb, 40000));

  CaptureLease usb;
  assert(usb.begin(CaptureLease::Owner::Usb, 5));
  assert(!usb.heartbeat(CaptureLease::Owner::Browser, 10000));
  assert(!usb.expired(20005));
  assert(usb.expired(20006));
  usb.clear();
  assert(usb.owner() == CaptureLease::Owner::None);
  CaptureLease device;
  assert(device.begin(CaptureLease::Owner::Device, 1));
  assert(!device.expired(24ULL*60*60*1000));
  assert(!device.heartbeat(CaptureLease::Owner::Browser, 100));
  assert(!device.heartbeat(CaptureLease::Owner::Usb, 100));
  device.clear();
  assert(device.begin(CaptureLease::Owner::Browser, 1000));
  assert(device.expired(21001));
  std::puts("Capture lease checks passed: browser/USB ownership, heartbeat renewal and expiry.");
}
