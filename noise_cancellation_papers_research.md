# Top 5 Most Cited Academic Papers for Noise Cancellation in Live Audio Streams

## 1. Phase-aware Speech Enhancement with Deep Complex U-Net (2019)
**Authors:** Hyeong-Seok Choi, Jang-Hyun Kim, Jaeha Huh, Adrian Kim, Jung-Woo Ha, Kyogu Lee  
**Published:** ICLR 2019  
**Citation Count:** Highly cited (exact count not available in search results)

### Key Algorithm/Approach:
- Deep Complex U-Net architecture incorporating complex-valued building blocks
- Polar coordinate-wise complex-valued masking method
- Novel weighted source-to-distortion ratio (wSDR) loss function

### Performance Metrics:
- Achieved state-of-the-art performance in all metrics at the time
- Significant improvements in SISNR, PESQ, and STOI scores
- Outperformed previous approaches by a large margin

### Implementation Complexity:
- Moderate complexity due to complex-valued operations
- 9 different code implementations available
- Suitable for real-time processing with proper optimization

---

## 2. DCCRN: Deep Complex Convolution Recurrent Network for Phase-Aware Speech Enhancement (2020)
**Authors:** Yanxin Hu, Yun Liu, Shubo Lv, Mengtao Xing, Shimin Zhang, Yihui Fu, Jian Wu, Bihong Zhang, Lei Xie  
**Published:** Interspeech 2020  
**Citation Count:** Highly cited (winner of DNS Challenge 2020)

### Key Algorithm/Approach:
- Combines CNN and RNN structures for complex-valued operations
- Integrates convolutional encoder-decoder (CED) with LSTM
- Phase-aware processing for better speech quality

### Performance Metrics:
- **DNS Challenge 2020:** 1st place (real-time track), 2nd place (non-real-time track)
- Only 3.7M parameters
- Superior MOS (Mean Opinion Score) ratings
- Outperforms CRN, GCRN in SI-SNR, PESQ, and STOI

### Implementation Complexity:
- Low complexity: 3.7M parameters
- Real-time capable
- Efficient for deployment in resource-constrained environments

---

## 3. PercepNet: A Perceptually-Motivated Approach for Low-Complexity Speech Enhancement (2020)
**Authors:** Jean-Marc Valin  
**Published:** ICASSP 2020  
**Citation Count:** Highly cited (foundation for PercepNet+ and other works)

### Key Algorithm/Approach:
- Combines signal processing with deep learning
- Uses knowledge of human perception
- Extension of RNNoise framework
- Real-time processing focus

### Performance Metrics:
- **DNS Challenge 2020:** 2nd place in real-time track
- Uses only 5% of a CPU core
- Runs on Raspberry Pi
- High-quality full-band speech enhancement

### Implementation Complexity:
- Ultra-low complexity
- 5% CPU usage
- Suitable for embedded devices
- Real-time performance guaranteed

---

## 4. Conv-TasNet: Surpassing Ideal Time-Frequency Magnitude Masking for Speech Separation (2019)
**Authors:** Yi Luo, Nima Mesgarani  
**Published:** IEEE/ACM Transactions on Audio, Speech, and Language Processing  
**Citation Count:** Highly cited (foundational work in time-domain processing)

### Key Algorithm/Approach:
- Fully-convolutional time-domain audio separation network
- Temporal convolutional network (TCN) with stacked 1-D dilated convolutions
- End-to-end time-domain processing
- No time-frequency transformation required

### Performance Metrics:
- Surpasses ideal time-frequency magnitude masks
- Superior performance in two- and three-speaker mixtures
- High scores in both objective and subjective evaluations
- SI-SNR improvements over baseline methods

### Implementation Complexity:
- Small model size
- Short minimum latency
- Suitable for both offline and real-time applications
- Efficient time-domain processing

---

## 5. Ultra Low Complexity Deep Learning Based Noise Suppression (2023)
**Authors:** [Authors not specified in search results]  
**Published:** arXiv 2023  
**Citation Count:** Recent paper, citations growing

### Key Algorithm/Approach:
- Two-stage processing framework
- Channelwise feature reorientation
- Modified power law compression
- Optimized for resource-constrained devices

### Performance Metrics:
- 3-4x reduction in computational complexity vs. state-of-the-art
- Comparable noise suppression performance
- Significantly reduced memory usage
- Maintains perceptual quality

### Implementation Complexity:
- Ultra-low complexity design
- 3-4x less computational load
- Ideal for edge devices
- Real-time capable on limited hardware

---

## Common Performance Metrics Across Papers:
1. **SI-SNR (Scale-Invariant Signal-to-Noise Ratio):** Measures signal quality improvement
2. **PESQ (Perceptual Evaluation of Speech Quality):** ITU-T standard for speech quality
3. **STOI (Short-Time Objective Intelligibility):** Measures speech intelligibility
4. **MOS (Mean Opinion Score):** Subjective quality assessment by human listeners
5. **Computational Complexity:** Parameters count, CPU/memory usage, latency

## Key Trends in Real-Time Audio Denoising:
1. **Phase-Aware Processing:** Modern approaches process both magnitude and phase
2. **Time-Domain Processing:** Direct processing without frequency transformation
3. **Hybrid Approaches:** Combining traditional DSP with deep learning
4. **Two-Stage Methods:** Noise suppression followed by speech restoration
5. **Complexity Reduction:** Focus on deployment in resource-constrained devices

## Implementation Considerations for Desktop Applications:
- Latency requirements: <10-20ms for real-time interaction
- CPU efficiency: Modern models achieve <5% CPU usage
- Memory footprint: Recent models optimize for <10MB runtime memory
- Quality vs. Complexity trade-off: PercepNet and DCCRN offer best balance
- Framework compatibility: Most models available in PyTorch/TensorFlow