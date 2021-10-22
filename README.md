# RIONetLib - C++
Windows Registered I/O API를 사용한 네트워크 라이브러리  
[Registered I/O  - Windows에서 고 성능 서버 만들기](https://docs.google.com/presentation/d/11PE5zKPtgCoxwfhNBWI1nM6gTuLRwWteZTQuX9bIHmM/edit?usp=sharing )    
  
[코딩 스타일](https://docs.google.com/document/d/1maR-thjNKaeWnbqdtGlUyjQvQT1Xuy_TMsP24LL4ctg/edit?usp=sharing )에 맞추어서 코딩하도록 한다.   
   
   
  
## TODO
- Echo 테스트: 패킷처리를 네트워크 스레드에서 하지 않고, 패킷 처리용 워커스레드에서 처리한다.
    - 이렇게 해야 recv와 send 스레드가 다르게 동작하고, 특히 세션이 동시에 recv와 send를 할 수 있다.
      
    
