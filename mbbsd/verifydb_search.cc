extern "C" {
#include "bbs.h"
#include "daemons.h"
#include "psb.h"
}

#ifdef USE_VERIFYDB

#include <set>
#include <string>
#include <vector>

#include "flatbuffers/flatbuffers.h"
#include "verifydb.h"
#include "verifydb.fbs.h"

class EntryBrowser {
public:
  static int SearchDisplay(int32_t vmethod, const char *vkey);

  int Header();
  int Footer();
  int Renderer(int i, int curr, int total, int rows);
  int InputProcessor(int key, int curr, int total, int rows);

private:
  Bytes buf_;
  std::vector<const VerifyDb::Entry *> entries_;
  std::string title_;

  EntryBrowser() {}
  EntryBrowser(const EntryBrowser &) = delete;
  EntryBrowser &operator=(EntryBrowser &) = delete;

  int Display();
};

extern "C" {

int EntryBrowserHeader(void *ctx) {
  return reinterpret_cast<EntryBrowser *>(ctx)->Header();
}
int EntryBrowserFooter(void *ctx) {
  return reinterpret_cast<EntryBrowser *>(ctx)->Footer();
}
int EntryBrowserRenderer(int i, int curr, int total, int rows, void *ctx) {
  return reinterpret_cast<EntryBrowser *>(ctx)->Renderer(i, curr, total, rows);
}
int EntryBrowserInputProcessor(int key, int curr, int total, int rows,
                               void *ctx) {
  return reinterpret_cast<EntryBrowser *>(ctx)->InputProcessor(key, curr, total,
                                                               rows);
}

}

int EntryBrowser::Display() {
  PSB_CTX ctx = {
      .curr = 0,
      .total = static_cast<int>(entries_.size()),
      .header_lines = 2,
      .footer_lines = 2,
      .allow_pbs_version_message = 1,
      .ctx = reinterpret_cast<void *>(this),
      .header = &EntryBrowserHeader,
      .footer = &EntryBrowserFooter,
      .renderer = &EntryBrowserRenderer,
      .input_processor = &EntryBrowserInputProcessor,
  };

  // If empty result, show a line about it.
  // Also PSB does not accept 0 as total.
  if (ctx.total == 0)
    ctx.total = 1;

  psb_main(&ctx);
  return 0;
}

int EntryBrowser::Header() {
  vs_hdr2barf(" �i�j�M�{�Ҹ�Ʈw�j \t %s", title_.c_str());
  // clang-format off
  outs(ANSI_COLOR(30;47));
  // clang-format on
  mvprints(1, 0, "   %-12s %-38s %-24s", "�ϥΪ̥N��", "�{�Ҹ�T", "�{�Үɶ�");
  outs(ANSI_RESET);
  return 0;
}

int EntryBrowser::Footer() {
  vs_footer(" �s����� ", " (����)���� (Enter/r/��)�ϥΪ̸�� "
                          "\t(q/��)���X");
  move(b_lines - 1, 0);
  return 0;
}

int EntryBrowser::Renderer(int i, int curr, int total, int rows) {
  if (curr >= entries_.size()) {
    outs("   �d�L���");
    return 0;
  }

  const auto *entry = entries_[i];
  time4_t ts = entry->timestamp();

  outs("   ");
  // clang-format off
  if (i == curr)
    outs(ANSI_COLOR(1;41;37));
  // clang-format on

  auto disp = FormatVerify(entry->vmethod(), entry->vkey());
  prints("%-12s%s %-38s %s", entry->userid()->c_str(), ANSI_RESET,
         disp ? disp->c_str() : "?", ts ? Cdate(&ts) : "(�L�ɶ�����)");
  return 0;
}

int EntryBrowser::InputProcessor(int key, int curr, int total, int rows) {
  if (curr >= entries_.size())
    return PSB_NA;

  switch (key) {
  case KEY_ENTER:
  case KEY_RIGHT:
  case 'r':
    if (user_info_admin(entries_[curr]->userid()->c_str()) < 0)
      vmsg("�䤣�즹�ϥΪ�");
    return PSB_NOP;
  }
  return PSB_NA;
}

// static
int EntryBrowser::SearchDisplay(int32_t vmethod, const char *vkey) {
  EntryBrowser browser;
  const VerifyDb::GetReply *reply;
  if (!verifydb_getverify(vmethod, vkey, &browser.buf_, &reply) ||
      !reply->ok() || !reply->entry()) {
    vmsg("�{�Ҩt�εL�k�ϥΡA�еy�ԦA�աC");
    return -1;
  }

  for (const auto *entry : *reply->entry()) {
    if (!is_entry_user_present(entry))
      continue;
    browser.entries_.push_back(entry);
  }

  if (auto title = FormatVerify(vmethod, vkey); title)
    browser.title_ = title.value();

  return browser.Display();
}

int verifydb_admin_search_display() {
  vs_hdr("�j�M�{�Ҹ�Ʈw");

  int y = 2;
  char email[EMAILSZ] = {};
  getdata_buf(y++, 0, "�{�ҫH�c�G", email, sizeof(email), DOECHO);
  strip_blank(email, email);
  str_lower(email, email);
  if (!*email)
    return 0;

  return EntryBrowser::SearchDisplay(VMETHOD_EMAIL, email);
}

#endif
