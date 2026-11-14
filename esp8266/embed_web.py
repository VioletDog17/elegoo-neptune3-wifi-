# SPDX-License-Identifier: GPL-3.0-or-later
Import("env")
from pathlib import Path
import gzip
project=Path(env.subst("$PROJECT_DIR"))
source=project.parent/"web-ui/index.html"
html=source.read_text(encoding="utf-8")
html=html.replace('<link rel="stylesheet" href="style.css">', '<style>'+source.with_name('style.css').read_text(encoding='utf-8')+'</style>')
html=html.replace('<script src="app.js"></script>', '<script>'+source.with_name('app.js').read_text(encoding='utf-8')+'</script>')
data=gzip.compress(html.encode('utf-8'),compresslevel=9,mtime=0)
out=project/"include/web_bundle.h"
body=",\n".join(",".join(str(x) for x in data[i:i+32]) for i in range(0,len(data),32))
out.write_text("#pragma once\n#include <pgmspace.h>\nconst uint8_t WEB_GZIP[] PROGMEM = {\n"+body+"\n};\n",encoding="ascii")
print("Embedded offline web UI:",len(data),"gzip bytes")
