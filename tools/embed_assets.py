"""Embed the device's local transfer page. Run after editing assets/transfer.html."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
page=(root/'assets/transfer.html').read_text()
assert ')PDAPAGE"' not in page
(root/'include/transfer_page.hpp').write_text('#pragma once\nnamespace tab5_pda {\ninline constexpr char transferPage[] = R"PDAPAGE('+page+')PDAPAGE";\n}\n')
