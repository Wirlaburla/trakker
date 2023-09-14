#ifndef TRAKKER_H
#define TRAKKER_H

void destroyWindows();
void createWindows();
void renderInfo(xmp_module_info *mi, xmp_frame_info *fi);
void renderAbout();
void renderTrack(xmp_module_info *mi, xmp_frame_info *fi);
void renderRows(xmp_module_info *mi, xmp_frame_info *fi);
void renderChannels(xmp_module_info *mi, xmp_frame_info *fi);
void renderInstruments(xmp_module_info *mi, xmp_frame_info *fi);
void generateEffectsTable(char* type);
void addToEffects(int id, char efx, bool mem);
bool isPartOf(char* w1, char* w2);

#endif