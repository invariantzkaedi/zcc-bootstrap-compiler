import os
import mido

base_dir = r'/mnt/h/_studio_tripo3d'
out_header = r'/mnt/g/zccMAIN/zcc/midi_data.h'

def extract_notes(filepath):
    if not os.path.exists(filepath):
        return []
    mid = mido.MidiFile(filepath)
    notes = []
    current_time = 0.0
    for msg in mid:
        current_time += msg.time
        if msg.type == 'note_on' and msg.velocity > 0:
            notes.append((current_time, msg.velocity))
    return notes

bass_notes = extract_notes(os.path.join(base_dir, 'Gemma 3.1 high AG (Bass).mid'))
drum_notes = extract_notes(os.path.join(base_dir, 'Gemma 3.1 high AG (Drums).mid'))
synth_notes = extract_notes(os.path.join(base_dir, 'Gemma 3.1 high AG (Synth).mid'))

with open(out_header, 'w') as f:
    f.write('#ifndef MIDI_DATA_H\n#define MIDI_DATA_H\n\n')
    
    f.write(f'int bass_count = {len(bass_notes)};\n')
    f.write('float bass_times[] = {')
    f.write(','.join([str(round(n[0], 3)) for n in bass_notes]))
    f.write('};\n')
    f.write('int bass_vels[] = {')
    f.write(','.join([str(n[1]) for n in bass_notes]))
    f.write('};\n\n')

    f.write(f'int drum_count = {len(drum_notes)};\n')
    f.write('float drum_times[] = {')
    f.write(','.join([str(round(n[0], 3)) for n in drum_notes]))
    f.write('};\n')
    f.write('int drum_vels[] = {')
    f.write(','.join([str(n[1]) for n in drum_notes]))
    f.write('};\n\n')

    f.write(f'int synth_count = {len(synth_notes)};\n')
    f.write('float synth_times[] = {')
    f.write(','.join([str(round(n[0], 3)) for n in synth_notes]))
    f.write('};\n')
    f.write('int synth_vels[] = {')
    f.write(','.join([str(n[1]) for n in synth_notes]))
    f.write('};\n\n')

    f.write('#endif\n')

print(f"Extracted {len(bass_notes)} bass, {len(drum_notes)} drums, {len(synth_notes)} synth events.")
