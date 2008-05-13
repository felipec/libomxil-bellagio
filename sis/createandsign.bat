@ECHO OFF

makesis -d%1 "..\src\loaders\loaders_symbian\symbianecomloader\sis\bellagioopenmaxsymbianloader.pkg" "%1\bellagioopenmaxsymbianloader.sis"
signsis -s "%1\bellagioopenmaxsymbianloader.sis" "%1\bellagioopenmaxsymbianloader.sis" %2 %3 %4

makesis -d%1 "..\src\components\components_symbian\outputstream\sis\bellagioopenmaxoutputstream.pkg" "%1\bellagioopenmaxoutputstream.sis"
signsis -s "%1\bellagioopenmaxoutputstream.sis" "%1\bellagioopenmaxoutputstream.sis" %2 %3 %4

makesis -d%1 "..\src\components\components_symbian\volume\sis\bellagioopenmaxvolume.pkg" "%1\bellagioopenmaxvolume.sis"
signsis -s "%1\bellagioopenmaxvolume.sis" "%1\bellagioopenmaxvolume.sis" %2 %3 %4

makesis -d%1 "..\src\components\components_symbian\audiomixer\sis\bellagioopenmaxaudiomixer.pkg" "%1\bellagioopenmaxaudiomixer.sis"
signsis -s "%1\bellagioopenmaxaudiomixer.sis" "%1\bellagioopenmaxaudiomixer.sis" %2 %3 %4

makesis -d%1 "..\test\components\components_symbian\symbianoutputstream\sis\OmxSymbianOutputStreamTest.pkg" "%1\OmxSymbianOutputStreamTest.sis"
signsis -s "%1\OmxSymbianOutputStreamTest.sis" "%1\OmxSymbianOutputStreamTest.sis" %2 %3 %4

makesis -d%1 "..\test\components\components_symbian\symbianaudiomixer\sis\OmxAudioMixerTest.pkg" "%1\OmxAudioMixerTest.sis"
signsis -s "%1\OmxAudioMixerTest.sis" "%1\OmxAudioMixerTest.sis" %2 %3 %4

makesis -d%1 "..\test\components\components_symbian\symbiancompnameenum\sis\OmxCompNameEnumTest.pkg" "%1\OmxCompNameEnumTest.sis"
signsis -s "%1\OmxCompNameEnumTest.sis" "%1\OmxCompNameEnumTest.sis" %2 %3 %4

makesis -d%1 "libomxil.pkg"
signsis -s "libomxil.sis" "libomxil.sis" %2 %3 %4

DEL "%1\bellagioopenmaxsymbianloader.sis"
DEL "%1\bellagioopenmaxvolume.sis"
DEL "%1\bellagioopenmaxaudiomixer.sis"
DEL "%1\bellagioopenmaxoutputstream.sis"
DEL "%1\OmxSymbianOutputStreamTest.sis"
DEL "%1\OmxAudioMixerTest.sis"
DEL "%1\OmxCompNameEnumTest.sis"


 