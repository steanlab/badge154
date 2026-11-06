const fs=require('fs'),vm=require('vm');
const html=fs.readFileSync('../docs/index.html','utf8');
const code=html.split('// ==CODEC== ')[1].split('// ==/CODEC==')[0].replace(/^[^\n]*\n/,'');
const ctx={};vm.createContext(ctx);vm.runInContext(code+';this.api={crc16,pack2,pack1,lz4,encodeImage}',ctx);
const {crc16,lz4,encodeImage,pack2}=ctx.api;
function unlz4(z,n){const o=new Uint8Array(n);let i=0,p=0;while(i<z.length){const t=z[i++];let l=t>>4;if(l===15){let b;do{b=z[i++];l+=b}while(b===255)}
 for(let k=0;k<l;k++)o[p++]=z[i++];if(i>=z.length)break;const off=z[i]|(z[i+1]<<8);i+=2;let m=t&15;if(m===15){let b;do{b=z[i++];m+=b}while(b===255)}m+=4;
 for(let k=0;k<m;k++){o[p]=o[p-off];p++}}if(p!==n)throw new Error('len '+p+' vs '+n);return o;}
let seed=7;const rnd=()=>{seed=(seed*1103515245+12345)&0x7fffffff;return seed/0x7fffffff;};
function mk(kind){const px=new Uint8Array(40000).fill(1);
 if(kind==='blocks')for(let k=0;k<40;k++){const x0=rnd()*200|0,y0=rnd()*200|0,w=5+rnd()*75|0,h=5+rnd()*75|0,c=rnd()*4|0;for(let y=y0;y<Math.min(200,y0+h);y++)for(let x=x0;x<Math.min(200,x0+w);x++)px[y*200+x]=c;}
 if(kind==='noise')for(let i=0;i<40000;i++)px[i]=rnd()*4|0;
 if(kind==='bw')for(let i=0;i<40000;i++)px[i]=rnd()<0.5?0:1;
 if(kind==='bwtext'){for(let y=0;y<200;y++)for(let x=0;x<200;x++)px[y*200+x]=((x>>3)+(y>>4))%3===0?0:1;}
 if(kind==='yr')for(let i=0;i<40000;i++)px[i]=rnd()<0.3?3:2;
 return px;}
const out={};
for(const k of ['flat','blocks','noise','bw','bwtext','yr']){
 const px=mk(k);const e=encodeImage(px,false);
 // decode check
 let raw=(e.flags&1)?unlz4(e.data,(e.flags&2)?5000:10000):e.data;
 let exp=pack2(px);let got;
 if(e.flags&2){got=new Uint8Array(10000);for(let i=0;i<40000;i++){const bit=(raw[i>>3]>>(7-(i&7)))&1;const c=bit?e.pal1:e.pal0;got[i>>2]|=c<<(6-2*(i&3));}}else got=raw;
 if(Buffer.compare(Buffer.from(got),Buffer.from(exp)))throw new Error('roundtrip fail '+k);
 console.log(k,'flags',e.flags,'pal',e.pal0,e.pal1,'bytes',e.data.length,'crc',e.crc.toString(16));
 out[k]={flags:e.flags,pal0:e.pal0,pal1:e.pal1,crc:e.crc,data:Buffer.from(e.data).toString('hex'),expect:Buffer.from(exp).toString('hex')};
 const f=encodeImage(px,true);if(f.flags&1)throw new Error('forceraw gave lz4');
}
fs.writeFileSync('codec_out.json',JSON.stringify(out));
console.log('crc16 check',crc16(Buffer.from('123456789')).toString(16),'(expect 29b1)');
