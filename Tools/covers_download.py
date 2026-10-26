#!/usr/bin/env python3
# Orbit, an Xbox 360 homebrew launcher.
# Licensed under the GNU General Public License v3.0 or later; see LICENSE.

# covers_download.py : fills a Covers folder with box art from XboxUnity, the same
# place Aurora gets its covers from. No account or API key is needed.
#
#   python covers_download.py library.txt                 titles from Orbit's library
#   python covers_download.py 4D5307E6 4D530004           or title IDs
#   python covers_download.py library.txt --out E:\Apps\Orbit\Covers
#   python covers_download.py library.txt --pick          choose each cover in the browser
#
# Each cover is the full 900x600 wrap (back | spine | front) Orbit expects, saved as
# <title ID>.jpg (or .png, as XboxUnity has it). Official covers win, then the best
# rated. Titles that already have a .jpg or .png in the folder are skipped unless
# --force is given. Python 3, no extra packages.
#
# --pick opens a page in the browser instead, listing every title with its current
# cover and the ones XboxUnity has for it (and for other title IDs of the same name,
# e.g. a game's Xbox Originals re-release), each with a preview. Clicking one saves it.

import argparse
import http.server
import json
import os
import re
import sys
import threading
import time
import urllib.parse
import urllib.request
import webbrowser

COVER_INFO = "http://xboxunity.net/Resources/Lib/CoverInfo.php?titleid={}"
COVER      = "http://xboxunity.net/Resources/Lib/Cover.php?cid={}&size=large"
THUMB      = "http://xboxunity.net/Resources/Lib/Cover.php?cid={}&size=small"
SEARCH     = "http://xboxunity.net/api/Covers/{}"
USER_AGENT = "Orbit-covers/1.0"
PAUSE      = 0.5   # seconds between titles, to go easy on XboxUnity

TITLE_ID = re.compile( r"^[0-9A-Fa-f]{8}$" )


def fetch( url, timeout=30 ):
    req = urllib.request.Request( url, headers={ "User-Agent": USER_AGENT } )
    with urllib.request.urlopen( req, timeout=timeout ) as r:
        return r.read()


# library.txt: shelf, title, version, size, launch, delete, drive, title ID (tab
# separated, "-" when unknown). Returns [(title ID, name)]
def read_library( path ):
    titles = []
    with open( path, encoding="latin-1" ) as f:
        for line in f:
            fields = line.rstrip( "\r\n" ).split( "\t" )
            if len( fields ) >= 8 and TITLE_ID.match( fields[7] ):
                titles.append( ( fields[7].upper(), fields[1] ) )
    return titles


# The cover to take: official first, then the highest rating, then the oldest upload
def best_cover( covers ):
    def key( c ):
        return ( c.get( "Official" ) == "1", int( c.get( "Rating" ) or 0 ), -int( c.get( "CoverID" ) or 0 ) )
    return max( covers, key=key ) if covers else None


# Saves a downloaded cover as <title ID>.jpg or .png (by its contents) and removes one
# with the other extension
def save_cover( out, tid, data ):
    if data.startswith( b"\xff\xd8" ):
        ext, other = ".jpg", ".png"
    elif data.startswith( b"\x89PNG" ):
        ext, other = ".png", ".jpg"
    else:
        raise ValueError( "not a JPEG or PNG" )
    with open( os.path.join( out, tid + ext ), "wb" ) as f:
        f.write( data )
    if os.path.exists( os.path.join( out, tid + other ) ):
        os.remove( os.path.join( out, tid + other ) )


def current_cover( out, tid ):
    for ext in ( ".jpg", ".png" ):
        if os.path.exists( os.path.join( out, tid + ext ) ):
            return os.path.join( out, tid + ext )
    return None

#-------------------------------------------------------------------------------------
# --pick : a page on 127.0.0.1 to choose each cover by eye
#-------------------------------------------------------------------------------------

# A cover offered for a title: its own (CoverInfo) or another title ID's with the same
# name (the name search). Newest first within each group, official ones on top.
def candidates( tid, name ):
    found, seen = [], set()
    try:
        info = json.loads( fetch( COVER_INFO.format( tid ) ) )
        for c in info.get( "Covers" ) or []:
            cid = int( c.get( "CoverID" ) or 0 )
            if cid and cid not in seen:
                seen.add( cid )
                found.append( { "cid": cid, "tid": tid, "name": name, "official": c.get( "Official" ) == "1",
                                "rating": c.get( "Rating" ), "user": c.get( "Username" ), "date": ( c.get( "UploadedDate" ) or "" )[:10] } )
    except Exception:
        pass
    if name and not TITLE_ID.match( name ):
        for c in search( name ):
            if c["name"].lower() == name.lower() and c["cid"] not in seen:
                seen.add( c["cid"] )
                found.append( c )
    found.sort( key=lambda c: c["date"], reverse=True )
    found.sort( key=lambda c: ( c["tid"] != tid, not c["official"] ) )
    return found


# XboxUnity's name search (matches anywhere in the name)
def search( query ):
    try:
        res = json.loads( fetch( SEARCH.format( urllib.parse.quote( query ) ) ) )
    except Exception:
        return []
    found = []
    for c in res if isinstance( res, list ) else []:
        m = re.search( r"(\d+)$", c.get( "url" ) or "" )
        if m:
            found.append( { "cid": int( m.group( 1 ) ), "tid": ( c.get( "titleid" ) or "" ).upper(), "name": c.get( "name" ) or "",
                            "official": bool( c.get( "official" ) ), "rating": c.get( "rating" ), "user": None,
                            "date": ( c.get( "uploaddate" ) or "" )[:10] } )
    return found


PAGE = """<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Orbit covers</title>
<style>
:root{color-scheme:dark;--bg:#101418;--card:#1a2027;--line:#2c343d;--text:#e6e9ec;--sub:#8d98a4;--accent:#5bd16a;--warn:#e0a040}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:14px/1.4 system-ui,sans-serif}
header{position:sticky;top:0;z-index:2;background:var(--bg);border-bottom:1px solid var(--line);padding:12px 16px;display:flex;gap:16px;align-items:center;flex-wrap:wrap}
header h1{font-size:16px;margin:0}header .sp{flex:1}
button{background:var(--card);color:var(--text);border:1px solid var(--line);border-radius:6px;padding:6px 12px;cursor:pointer;font:inherit}
button.go{background:var(--accent);color:#08210d;border-color:var(--accent);font-weight:600}
main{padding:16px;display:grid;grid-template-columns:minmax(0,1fr);gap:16px}
.t{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:12px}
.t h2{font-size:15px;margin:0 0 2px}.t .id{color:var(--sub);font-family:monospace}
.row{display:flex;gap:12px;overflow-x:auto;padding:10px 0 4px}
.c{flex:0 0 auto;width:210px;cursor:pointer;border:2px solid transparent;border-radius:6px;padding:4px}
.c:hover{border-color:var(--line)}.c.cur{cursor:default;border-color:var(--accent)}
.c img{width:100%;aspect-ratio:3/2;object-fit:contain;background:#000;border-radius:4px;display:block}
.c .m{font-size:12px;color:var(--sub);margin-top:4px}.b{display:inline-block;font-size:11px;padding:0 5px;border-radius:3px;margin-right:4px;font-weight:600}
.b.o{background:var(--accent);color:#08210d}.b.x{background:var(--warn);color:#2a1a00}
.none{color:var(--sub);padding:10px 0}.s{display:flex;gap:8px;margin-top:6px}.s input{flex:1;min-width:0;background:var(--bg);color:var(--text);border:1px solid var(--line);border-radius:6px;padding:6px 8px;font:inherit}
#v{position:fixed;inset:0;background:rgba(0,0,0,.85);display:none;flex-direction:column;align-items:center;justify-content:center;gap:12px;padding:16px;z-index:5}
#v img{max-width:100%;max-height:80vh;background:#000}#v .a{display:flex;gap:12px}
</style></head><body>
<header><h1>Orbit covers</h1><label><input type="checkbox" id="missing"> only titles without a cover</label><span class="sp"></span><span id="st"></span><button class="go" id="done">Done</button></header>
<main id="list"></main>
<div id="v"><img id="vi" alt=""><div id="vt"></div><div class="a"><button class="go" id="use">Use this cover</button><button id="close">Cancel</button></div></div>
<script>
const $=s=>document.querySelector(s),esc=s=>String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
let pick=null;
function meta(c,tid){let m='';if(c.official)m+='<span class="b o">OFFICIAL</span>';if(c.tid!==tid)m+='<span class="b x">'+esc(c.tid)+'</span>';
 m+=(c.rating!=null?'rating '+esc(c.rating)+' \\u00b7 ':'')+esc(c.date||'')+(c.user?' \\u00b7 '+esc(c.user):'');if(c.tid!==tid)m+='<br>'+esc(c.name);return m}
function tiles(el,list,t){if(!list.length){el.innerHTML+='<div class="none">No covers found.</div>';return}
 for(const c of list){const d=document.createElement('div');d.className='c';d.innerHTML='<img loading="lazy" src="/thumb?cid='+c.cid+'" alt=""><div class="m">'+meta(c,t.tid)+'</div>';
  d.onclick=()=>{pick={t,c};$('#vi').src='/full?cid='+c.cid;$('#vt').innerHTML=esc(t.name)+' \\u2014 cover '+c.cid;$('#v').style.display='flex'};el.appendChild(d)}}
async function load(t,card){const r=card.querySelector('.row');r.innerHTML='<div class="none">Loading\\u2026</div>';
 const list=await (await fetch('/api/covers?tid='+t.tid+'&name='+encodeURIComponent(t.name))).json();r.innerHTML='';cur(t,r);tiles(r,list,t)}
function cur(t,r){const d=document.createElement('div');d.className='c cur';d.innerHTML=t.have?'<img src="/current?tid='+t.tid+'&v='+Date.now()+'" alt=""><div class="m"><span class="b o">CURRENT</span></div>':'<div class="none" style="width:100%;aspect-ratio:3/2;display:grid;place-items:center;border:1px dashed var(--line);border-radius:4px">no cover yet</div><div class="m">CURRENT</div>';r.appendChild(d)}
async function main(){const titles=await (await fetch('/api/titles')).json(),list=$('#list');
 const io=new IntersectionObserver(es=>{for(const e of es)if(e.isIntersecting){io.unobserve(e.target);load(e.target._t,e.target)}},{rootMargin:'400px'});
 for(const t of titles){const card=document.createElement('section');card.className='t';card._t=t;
  card.innerHTML='<h2>'+esc(t.name||t.tid)+'</h2><span class="id">'+t.tid+'</span><div class="row"></div><div class="s"><input placeholder="Search XboxUnity by name"><button>Search</button></div>';
  const inp=card.querySelector('input');inp.value=/^[0-9A-Fa-f]{8}$/.test(t.name)?'':t.name;
  const go=async()=>{if(!inp.value.trim())return;const r=card.querySelector('.row');r.innerHTML='<div class="none">Searching\\u2026</div>';
   const l=await (await fetch('/api/search?q='+encodeURIComponent(inp.value.trim()))).json();r.innerHTML='';cur(t,r);tiles(r,l,t)};
  card.querySelector('.s button').onclick=go;inp.onkeydown=e=>{if(e.key==='Enter')go()};
  list.appendChild(card);io.observe(card)}
 $('#missing').onchange=e=>{for(const c of list.children)c.style.display=e.target.checked&&c._t.have?'none':''};
 $('#st').textContent=titles.length+' titles'}
$('#close').onclick=()=>{$('#v').style.display='none'};
$('#use').onclick=async()=>{const {t,c}=pick;$('#use').disabled=true;
 const r=await fetch('/api/save',{method:'POST',body:JSON.stringify({tid:t.tid,cid:c.cid})});$('#use').disabled=false;
 if(!r.ok){alert('Could not save: '+await r.text());return}
 t.have=true;$('#v').style.display='none';for(const card of $('#list').children)if(card._t===t){const cd=card.querySelector('.c.cur');const n=document.createElement('div');cur(t,n);cd.replaceWith(n.firstChild)}};
$('#done').onclick=async()=>{await fetch('/api/quit',{method:'POST'});document.body.innerHTML='<main><p>Done. You can close this tab.</p></main>'};
main();
</script></body></html>
"""


def pick( titles, out ):
    thumbs = {}
    lock = threading.Lock()

    class Handler( http.server.BaseHTTPRequestHandler ):
        def log_message( self, *a ):
            pass

        def send( self, body, ctype="application/json", code=200 ):
            if isinstance( body, str ):
                body = body.encode()
            self.send_response( code )
            self.send_header( "Content-Type", ctype )
            self.send_header( "Content-Length", str( len( body ) ) )
            self.send_header( "Cache-Control", "no-store" )
            self.end_headers()
            self.wfile.write( body )

        def image( self, data ):
            self.send( data, "image/png" if data.startswith( b"\x89PNG" ) else "image/jpeg" )

        def do_GET( self ):
            url = urllib.parse.urlparse( self.path )
            q = { k: v[0] for k, v in urllib.parse.parse_qs( url.query ).items() }
            try:
                if url.path == "/":
                    self.send( PAGE, "text/html; charset=utf-8" )
                elif url.path == "/api/titles":
                    self.send( json.dumps( [ { "tid": tid, "name": name, "have": current_cover( out, tid ) is not None } for tid, name in titles ] ) )
                elif url.path == "/api/covers":
                    self.send( json.dumps( candidates( q["tid"].upper(), q.get( "name", "" ) ) ) )
                elif url.path == "/api/search":
                    self.send( json.dumps( search( q["q"] ) ) )
                elif url.path in ( "/thumb", "/full" ):
                    cid = int( q["cid"] )
                    if url.path == "/full":
                        return self.image( fetch( COVER.format( cid ) ) )
                    with lock:
                        data = thumbs.get( cid )
                    if data is None:
                        data = fetch( THUMB.format( cid ) )
                        with lock:
                            thumbs[cid] = data
                    self.image( data )
                elif url.path == "/current":
                    path = current_cover( out, q["tid"].upper() ) if TITLE_ID.match( q["tid"] ) else None
                    if not path:
                        return self.send( "no cover", "text/plain", 404 )
                    with open( path, "rb" ) as f:
                        self.image( f.read() )
                else:
                    self.send( "not found", "text/plain", 404 )
            except Exception as e:
                self.send( str( e ), "text/plain", 502 )

        def do_POST( self ):
            body = self.rfile.read( int( self.headers.get( "Content-Length" ) or 0 ) )
            if self.path == "/api/quit":
                self.send( "{}" )
                threading.Thread( target=server.shutdown ).start()
                return
            if self.path != "/api/save":
                return self.send( "not found", "text/plain", 404 )
            try:
                req = json.loads( body )
                tid, cid = req["tid"].upper(), int( req["cid"] )
                if not TITLE_ID.match( tid ):
                    raise ValueError( "bad title ID" )
                save_cover( out, tid, fetch( COVER.format( cid ) ) )
                print( "  saved  {} (cover {})".format( tid, cid ) )
                self.send( "{}" )
            except Exception as e:
                self.send( str( e ), "text/plain", 502 )

    server = http.server.ThreadingHTTPServer( ( "127.0.0.1", 0 ), Handler )
    url = "http://127.0.0.1:{}/".format( server.server_address[1] )
    print( "Pick the covers at {}  (Done on the page, or Ctrl+C here, to finish)".format( url ), flush=True )
    webbrowser.open( url )
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    server.server_close()
    print( "Saved in {}. Copy the folder to Orbit's own folder on the console, then SCAN NOW in SETTINGS.".format( os.path.abspath( out ) ) )


def main():
    ap = argparse.ArgumentParser( description="Download Orbit box art from XboxUnity." )
    ap.add_argument( "sources", nargs="+", help="Orbit's library.txt and / or title IDs (e.g. 4D5307E6)" )
    ap.add_argument( "--out", default="Covers", help="folder to save the covers in (default: Covers)" )
    ap.add_argument( "--force", action="store_true", help="download again even if the cover is already there" )
    ap.add_argument( "--pick", action="store_true", help="choose each title's cover from a page in the browser" )
    args = ap.parse_args()

    titles = []
    for s in args.sources:
        if TITLE_ID.match( s ):
            titles.append( ( s.upper(), "" ) )
        elif os.path.isfile( s ):
            titles += read_library( s )
        else:
            sys.exit( "Not a title ID or a file: {}".format( s ) )

    # Each title once (the library can list a title ID twice, e.g. a game and its demo)
    seen = set()
    titles = [ t for t in titles if not ( t[0] in seen or seen.add( t[0] ) ) ]
    if not titles:
        sys.exit( "No title IDs found." )

    os.makedirs( args.out, exist_ok=True )
    if args.pick:
        return pick( titles, args.out )
    got = have = missing = failed = 0
    for tid, name in titles:
        label = "{} {}".format( tid, name ).strip()
        if not args.force and any( os.path.exists( os.path.join( args.out, tid + ext ) ) for ext in ( ".jpg", ".png" ) ):
            print( "  have   " + label )
            have += 1
            continue
        try:
            info = json.loads( fetch( COVER_INFO.format( tid ) ) )
            cover = best_cover( info.get( "Covers" ) or [] )
            if not cover:
                print( "  none   " + label )
                missing += 1
                continue
            # Most covers are JPEG, a few PNG; Orbit takes either
            save_cover( args.out, tid, fetch( COVER.format( cover["CoverID"] ) ) )
            print( "  got    " + label )
            got += 1
        except Exception as e:
            print( "  FAILED {} ({})".format( label, e ) )
            failed += 1
        time.sleep( PAUSE )

    print( "\n{} downloaded, {} already there, {} with no cover on XboxUnity, {} failed. Saved in {}".format(
        got, have, missing, failed, os.path.abspath( args.out ) ) )
    print( "Copy the folder to Orbit's own folder on the console, then SCAN NOW in SETTINGS." )


if __name__ == "__main__":
    main()
