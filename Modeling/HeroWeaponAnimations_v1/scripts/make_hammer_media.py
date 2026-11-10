from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
P=Path(__file__).resolve().parents[1];V=P/'previews'
font=lambda n:ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',n)
seq=[Image.open(f).convert('RGB') for f in sorted((V/'_frames/warhammer_uppercut').glob('*.png'))]
assert len(seq)>5
seq[0].save(V/'hero_warhammer_uppercut.gif',save_all=True,append_images=seq[1:],duration=83,loop=0,optimize=False)
before=Image.open(P/'revisions/20261105_before_hammer_grip/hero_warhammer.gif').convert('RGB')
after=Image.open(V/'HERO_Hammer_Ready.png').convert('RGB')
im=Image.new('RGB',(960,570),(23,26,32));d=ImageDraw.Draw(im)
d.text((18,12),'망치 파지 수정 · 준비 자세 비교',font=font(25),fill=(238,227,204))
im.paste(before.resize((480,480)),(0,55));im.paste(after.resize((480,480)),(480,55))
d.text((18,538),'수정 전 · 얼굴 앞의 망치 / 꺾인 손목',font=font(18),fill=(220,187,168))
d.text((498,538),'수정 후 · 몸 옆으로 이동 / 손목 정렬',font=font(18),fill=(184,215,190))
im.save(V/'hammer_grip_before_after.jpg',quality=94)
im=Image.new('RGB',(1280,380),(23,26,32));d=ImageDraw.Draw(im)
for i,(n,label) in enumerate([('Ready','준비'),('Smash','내려치기'),('Uppercut','올려치기'),('Guard','방어')]):
 frame=Image.open(V/('HERO_Hammer_'+n+'.png')).convert('RGB').resize((320,320))
 im.paste(frame,(i*320,35));d.text((i*320+12,7),label,font=font(20),fill=(238,227,204))
d.text((15,357),'양손 간격 0.30m · 손잡이를 가로지르는 주먹 · 전완과 손목 정렬',font=font(16),fill=(180,194,207))
im.save(V/'hammer_grip_four_actions.jpg',quality=94)
print('Created hammer comparison, four poses and uppercut GIF')
