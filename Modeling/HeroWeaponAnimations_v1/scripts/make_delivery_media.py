from PIL import Image,ImageDraw,ImageFont,ImageSequence
from pathlib import Path
import json,math
P=Path(__file__).resolve().parents[1];V=P/'previews'
fontpath='C:/Windows/Fonts/malgun.ttf'
font=lambda n:ImageFont.truetype(fontpath,n)
names={'staff':'지팡이 · 전방 시전','bow':'활 · 장전 / 조준 / 발사','mace':'철퇴 · 내려치기','axe':'도끼 · 사선 내려치기','warhammer':'전투 망치 · 양손 강타','spear':'창 · 양손 찌르기','dagger':'단검 · 빠른 찌르기'}
targets={'staff':'HERO_Staff_CastBolt','mace':'HERO_Mace_Strike','axe':'HERO_Axe_Chop','warhammer':'HERO_Hammer_Smash','spear':'HERO_Spear_Thrust','dagger':'HERO_Dagger_Stab','bow':'HERO_Bow_Draw'}
all_frames={}
for kind in names:
 frames=[Image.open(f).convert('RGB') for f in sorted((V/'_frames'/kind).glob('*.png'))]
 fresh=bool(frames)
 if not frames:
  with Image.open(V/('hero_'+kind+'.gif')) as old:frames=[f.convert('RGB').copy() for f in ImageSequence.Iterator(old)]
 all_frames[kind]=frames
 if not fresh:continue
 frames[0].save(V/('hero_'+kind+'.gif'),save_all=True,append_images=frames[1:],duration=83,loop=0,optimize=False)
 sheet=Image.new('RGB',(1440,280),(24,27,33));d=ImageDraw.Draw(sheet)
 for i in range(6):
  frame=frames[round(i*(len(frames)-1)/5)].resize((240,240),Image.Resampling.LANCZOS);sheet.paste(frame,(i*240,40))
 d.text((14,7),names[kind]+' — 주요 프레임',font=font(19),fill=(234,223,201))
 sheet.save(V/(kind+'_keyframes.jpg'),quality=92)

overview=Image.new('RGB',(1536,900),(23,26,32));d=ImageDraw.Draw(overview)
d.text((28,16),'주인공 무기 애니메이션 · 7종 / 새 동작 30개',font=font(29),fill=(240,229,205))
d.text((28,59),'60fps 키프레임 · 공통 걷기 / 달리기 유지 · 실제 GLB 재임포트 렌더',font=font(17),fill=(161,171,184))
for i,(kind,name) in enumerate(names.items()):
 x=(i%4)*384;y=100+(i//4)*400
 im=Image.open(V/(targets[kind]+'.png')).convert('RGB').resize((354,354),Image.Resampling.LANCZOS)
 overview.paste(im,(x+15,y));d.text((x+15,y+363),name,font=font(20),fill=(228,217,193))
d.text((1170,548),'추가 동작',font=font(28),fill=(240,229,205))
for j,line in enumerate(['준비 자세 / 방어','보조 공격 / 시전','집중 / 장전 / 조준','시위·활대 키프레임','접촉·발사 이벤트']):d.text((1170,600+j*39),line,font=font(20),fill=(175,187,202))
overview.save(V/'animations_overview.jpg',quality=94)

cat=json.loads((P/'animation_catalog.json').read_text('utf-8'))
specs=cat['new_clips'];sheet=Image.new('RGB',(1500,math.ceil(len(specs)/5)*300+80),(24,27,33));d=ImageDraw.Draw(sheet)
d.text((20,14),'주인공 새 무기 동작 30개 · 대표 프레임',font=font(27),fill=(235,225,204))
for i,s in enumerate(specs):
 x=(i%5)*300;y=70+(i//5)*300
 im=Image.open(V/(s['name']+'.png')).convert('RGB').resize((260,260),Image.Resampling.LANCZOS);sheet.paste(im,(x+20,y))
 d.text((x+7,y+264),s['name'].removeprefix('HERO_'),font=font(17),fill=(223,217,202))
sheet.save(V/'all_clips_contact.jpg',quality=94)

animated=[]
for f in range(36):
 canvas=Image.new('RGB',(1024,656),(23,26,32));d=ImageDraw.Draw(canvas)
 d.text((14,7),'무기별 키프레임 애니메이션',font=font(22),fill=(240,228,201))
 for i,(kind,title) in enumerate(names.items()):
  x=(i%4)*256;y=43+(i//4)*302
  seq=all_frames[kind];im=seq[f%len(seq)].resize((252,252),Image.Resampling.LANCZOS);canvas.paste(im,(x+2,y));d.text((x+8,y+268),title,font=font(13),fill=(223,217,202))
 d.text((785,398),'7종 · 새 동작 30개',font=font(19),fill=(240,228,201));d.text((785,440),'공통 Walk / Run 유지',font=font(15),fill=(178,188,200))
 animated.append(canvas)
animated[0].save(V/'animations_overview.gif',save_all=True,append_images=animated[1:],duration=83,loop=0,optimize=False)
print('Created seven motion GIFs, keyframe sheets and overviews')
