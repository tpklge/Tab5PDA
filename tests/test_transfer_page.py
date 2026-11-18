"""Execute the actual embedded transfer-page JavaScript using QuickJS and a minimal DOM."""
import json
import re
import unittest
import zlib
from pathlib import Path
import quickjs
ROOT=Path(__file__).resolve().parents[1]

class TransferPage(unittest.TestCase):
    def setUp(self):
        self.context=quickjs.Context()
        def row(value):return json.dumps(value,ensure_ascii=False,separators=(',',':'))+'\n'
        text=row(dict(format='pda-backup-v1',schema='2',records='1',nextId='2'))
        text+=row(dict(type='settings',brightness='160',volume='100',timeout='60',offset='480',lastClock=''))
        text+=row(dict(type='record',id='1',kind='4',title='Nota café',detail='Texto original',when='',completed='0',notebook='Trabalho'))
        text+=row(dict(type='commit',crc32=f'{zlib.crc32(text.encode())&0xffffffff:08x}'))
        self.context.add_callable('_fixture',lambda:text)
        self.context.add_callable('_encode',lambda value:json.dumps(list(value.encode('utf-8'))))
        self.context.eval('''
            class TextEncoder{encode(text){return JSON.parse(_encode(text))}}
            function element(){return {value:'',textContent:'',disabled:false,hidden:false,files:[],options:[],children:[],append(child){this.children.push(child);this.options.push(child)},replaceChildren(){this.children=[];this.options=[]},click(){}}}
            const elements={};globalThis.document={getElementById(id){return elements[id]||(elements[id]=element())},createElement(){return element()}};
            globalThis.setTimeout=()=>0;globalThis.URL={createObjectURL:()=>'',revokeObjectURL(){}};
            globalThis.File=class{constructor(parts,name){this.parts=parts;this.name=name}};
            globalThis.FormData=class{constructor(){this.data=[]}append(key,file){this.data.push(file)}};
            globalThis.captured='';globalThis.requestCode='';
            globalThis.fetch=async(path,options)=>{requestCode=options.headers['X-PDA-Token'];let text='';if(path.startsWith('/download'))text=_fixture();else if(path.startsWith('/upload')){captured=options.body.data[0].parts.join('');text='Preparado no SD: incoming-test-sync-edited.jsonl'}return {ok:true,text:async()=>text,blob:async()=>({})}};
        ''')
        html=(ROOT/'assets/transfer.html').read_text()
        embedded=(ROOT/'include/transfer_page.hpp').read_text()
        self.assertIn(html,embedded)
        script=re.search(r'<script>(.*?)</script>',html,re.S).group(1)
        self.context.eval(script)
        self.context.eval("$('token').value='session-test';openSync();")
        self.pump()
    def pump(self):
        for _ in range(1000):
            if not self.context.execute_pending_job():return
        self.fail('JavaScript did not settle')
    def test_crc_unicode_and_roundtrip(self):
        self.assertEqual(self.context.eval("crc32('ação\\n')"),f'{zlib.crc32("ação\n".encode())&0xffffffff:08x}')
        self.assertEqual(self.context.eval('records[0].title'),'Nota café')
        self.context.eval("$('title').value='Editada no computador';$('detail').value='Ação e café';add(3);$('title').value='Comprar leite';send();")
        self.pump()
        text=self.context.eval('captured')
        rows=[json.loads(line) for line in text.splitlines()]
        self.assertEqual(rows[0]['records'],'2')
        self.assertEqual(rows[2]['title'],'Editada no computador')
        self.assertEqual(rows[2]['notebook'],'Trabalho')
        self.assertEqual(rows[3]['title'],'Comprar leite')
        self.assertEqual(rows[3]['kind'],'3')
        prefix=text[:text.rfind('{"type":"commit"')]
        self.assertEqual(rows[-1]['crc32'],f'{zlib.crc32(prefix.encode())&0xffffffff:08x}')
        self.assertEqual(self.context.eval('requestCode'),'session-test')
    def test_soft_delete(self):
        self.context.eval('trash();send();');self.pump()
        rows=[json.loads(line) for line in self.context.eval('captured').splitlines()]
        self.assertEqual(rows[2]['trashed'],'1')
        self.assertEqual(rows[2]['detail'],'Texto original')

if __name__=='__main__':unittest.main()
