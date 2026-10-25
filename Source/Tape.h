// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
namespace pocket {
// Four mono tracks. Allocation, clearing and state copies happen outside process().
class Tape {
public:
    void prepare(double sr) {
        for(auto& markers:cuts)if(markers.capacity()<1024)markers.reserve(1024);
        if(rate!=sr){clipboard={};undoTracks={};hasUndo=false;}
        if(rate!=sr||tracks[0].empty()) {
            for(auto& t:tracks) {
                std::vector<float> next(size_t(sr*16),0);
                if(!t.empty()) for(size_t i=0;i<next.size();++i) {
                    const double pos=double(i)*rate/sr; const auto a=std::min(size_t(pos),t.size()-1),b=std::min(a+1,t.size()-1);
                    next[i]=t[a]+float(pos-std::floor(pos))*(t[b]-t[a]);
                }
                t=std::move(next);
            }
            rate=sr;
        }
        position=0;
    }
    void clear(int track) {if(track>=0&&track<4){backup();std::fill(tracks[size_t(track)].begin(),tracks[size_t(track)].end(),0);cuts[size_t(track)].clear();}}
    void finishRecording(){if(wasRecording){mark(recordingTrack,recordEnd);wasRecording=false;}}
    void rewind() { position=0; }
    void seek(double seconds) {position=std::clamp(seconds*rate,0.0,tracks[0].empty()?0.0:double(tracks[0].size()-1));}
    void setRegion(bool enabled,double in,double out) {regionEnabled=enabled;regionIn=in;regionOut=out;}
    void setChop(bool enabled,double bpm) {if(enabled&&!chopping)chopStart=position;chopping=enabled;chopSize=rate*60/std::clamp(bpm,40.0,300.0)/4;}
    const std::array<std::vector<double>,4>& getCuts() const {return cuts;}
    void setCuts(std::array<std::vector<double>,4> markers) {cuts=std::move(markers);for(auto& track:cuts){track.erase(std::remove_if(track.begin(),track.end(),[](double x){return !std::isfinite(x)||x<=0||x>=16;}),track.end());std::sort(track.begin(),track.end());track.erase(std::unique(track.begin(),track.end()),track.end());track.reserve(1024);}}
    std::pair<size_t,size_t> take(int track,double seconds,bool useRegion=false) const {
        const size_t length=std::min(size_t(rate*seconds),tracks[size_t(track)].size());
        if(useRegion&&regionOut>regionIn) return {std::min(size_t(regionIn*rate),length),std::min(size_t(regionOut*rate),length)};
        const size_t head=length?std::min(size_t(std::max(0.0,position-1)),length-1):0;
        size_t first=0,last=length;for(double cut:cuts[size_t(track)]) {const auto point=size_t(cut*rate);if(point<=head)first=std::min(point,length);else {last=std::min(point,length);break;}}return {first,last};
    }
    bool split(int track,bool join,double seconds) {
        auto& markers=cuts[size_t(track)];const double now=getPosition();
        if(join) {if(markers.empty())return false;backup();auto after=std::upper_bound(markers.begin(),markers.end(),now+1/rate);if(after!=markers.end())after=markers.erase(after);auto before=std::lower_bound(markers.begin(),markers.end(),now+1/rate);if(before!=markers.begin())markers.erase(std::prev(before));return true;}
        if(now<=0||now>=seconds)return false;backup();auto point=std::lower_bound(markers.begin(),markers.end(),now);if(point==markers.end()||std::abs(*point-now)>1/rate)markers.insert(point,now);return true;
    }
    bool slide(int track,double delta,double seconds) {
        const auto range=take(track,seconds);const auto distance=int64_t(std::llround(delta*rate));if(!distance||range.second<=range.first)return false;
        const auto begin=int64_t(range.first)+distance,end=int64_t(range.second)+distance;const auto limit=std::min(size_t(rate*seconds),tracks[size_t(track)].size());if(begin<0||end>int64_t(limit))return false;
        auto& channel=tracks[size_t(track)];for(size_t i=size_t(begin);i<size_t(end);++i)if((i<range.first||i>=range.second)&&std::abs(channel[i])>.0000001f)return false;
        std::vector<float> audio(channel.begin()+range.first,channel.begin()+range.second);backup();std::fill(channel.begin()+range.first,channel.begin()+range.second,0);std::copy(audio.begin(),audio.end(),channel.begin()+begin);
        auto& markers=cuts[size_t(track)];const double first=double(range.first)/rate,last=double(range.second)/rate;markers.erase(std::remove_if(markers.begin(),markers.end(),[&](double x){return std::abs(x-first)<1/rate||std::abs(x-last)<1/rate;}),markers.end());mark(track,double(begin)/rate);mark(track,double(end)/rate);position=double(begin);return true;
    }
    bool lift(int track,bool all,double seconds,bool region) {
        const auto range=take(track,seconds,region);if(range.second<=range.first)return false;
        backup();clipboard={};clipboardTrack=track;clipboardAll=all;
        for(int i=0;i<4;++i)if(all||i==track){auto& src=tracks[size_t(i)];clipboard[size_t(i)].assign(src.begin()+range.first,src.begin()+range.second);std::fill(src.begin()+range.first,src.begin()+range.second,0);}
        position=double(range.first);return true;
    }
    bool drop(int track,double seconds) {
        if(clipboard[size_t(clipboardTrack)].empty())return false;
        const size_t length=std::min(size_t(rate*seconds),tracks[size_t(track)].size()),start=std::min(size_t(position),length);
        const auto count=std::min(clipboard[size_t(clipboardTrack)].size(),length-start);if(count==0)return false;backup();
        for(int i=0;i<4;++i)if(clipboardAll||i==track){const auto& source=clipboard[size_t(clipboardAll?i:clipboardTrack)];if(source.size()>=count)std::copy_n(source.begin(),count,tracks[size_t(i)].begin()+start);}
        auto& markers=cuts[size_t(track)];for(double point:{double(start)/rate,double(start+count)/rate})if(point>0&&point<seconds)markers.push_back(point);std::sort(markers.begin(),markers.end());markers.erase(std::unique(markers.begin(),markers.end()),markers.end());position=std::min(double(start+count),double(length-1));return true;
    }
    bool undo() {if(!hasUndo)return false;tracks.swap(undoTracks);cuts.swap(undoCuts);return true;}
    std::vector<float> clipboardMono() const {
        auto result=clipboard[size_t(clipboardTrack)];if(clipboardAll)for(size_t i=0;i<result.size();++i){float sum=0;for(const auto& channel:clipboard)if(i<channel.size())sum+=channel[i];result[i]=std::clamp(sum,-1.f,1.f);}return result;
    }
    float process(float input,bool play,bool record,bool overdub,int selected,float speed,float seconds,const std::array<float,4>& gains,float recordGain=1) {
        if(wasRecording&&(!record||!play||selected!=recordingTrack)){mark(recordingTrack,recordEnd);wasRecording=false;}
        if(!play||tracks[0].empty()) return input;
        const auto length=std::clamp(size_t(rate*seconds),size_t(2),tracks[0].size());
        double first=0,last=double(length);
        if(regionEnabled) {first=std::clamp(regionIn*rate,0.0,double(length-2));last=std::clamp(regionOut*rate,first+2,double(length));}
        if(chopping) {first=std::clamp(chopStart,first,last-2);last=std::min(last,first+std::max(2.0,chopSize));}
        const double span=last-first;if(position<first||position>=last){position=first+std::fmod(position-first,span);if(position<first)position+=span;}
        const auto a=size_t(position),b=a+1>=size_t(last)?size_t(first):a+1; const float fraction=float(position-double(a));
        if(record&&!wasRecording){recordingTrack=selected;mark(selected,position/rate);wasRecording=true;}
        float mixed=input;
        for(size_t i=0;i<4;++i) {
            const float old=tracks[i][a]+fraction*(tracks[i][b]-tracks[i][a]);
            if(!(record&&!overdub&&int(i)==selected)) mixed+=old*gains[i];
            if(record&&int(i)==selected) tracks[i][a]=std::clamp(input*recordGain+(overdub?tracks[i][a]:0),-1.f,1.f);
        }
        position+=record?1.0:double(speed);if(record)recordEnd=position/rate;
        return std::clamp(mixed,-1.f,1.f);
    }
    double getPosition() const { return position/rate; }
    double getRate() const { return rate; }
    const std::array<std::vector<float>,4>& getTracks() const { return tracks; }
    void restore(std::array<std::vector<float>,4> data,double sr) { tracks=std::move(data);cuts={};clipboard={};undoTracks={};hasUndo=false; rate=sr; position=0; }
private:
    void mark(int track,double seconds){if(track<0||track>3||seconds<=0||seconds>=16)return;auto& points=cuts[size_t(track)];if(points.size()>=1024)return;auto at=std::lower_bound(points.begin(),points.end(),seconds);if(at==points.end()||std::abs(*at-seconds)>1/rate)points.insert(at,seconds);}
    bool wasRecording=false;int recordingTrack=0;double recordEnd=0;
    void backup(){undoTracks=tracks;for(auto& points:undoCuts)points.reserve(1024);undoCuts=cuts;hasUndo=true;}
    std::array<std::vector<float>,4> undoTracks,clipboard;
    std::array<std::vector<double>,4> cuts,undoCuts;
    bool hasUndo=false,clipboardAll=false,regionEnabled=false,chopping=false;
    int clipboardTrack=0;
    double regionIn=0,regionOut=0,chopStart=0,chopSize=1;
    std::array<std::vector<float>,4> tracks;
    double rate=48000,position=0;
};
}
