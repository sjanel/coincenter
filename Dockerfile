FROM alpine:latest

RUN apk --no-cache add cmake make gcc g++ libc-dev linux-headers git bash zlib-dev curl-dev

ADD . /service
WORKDIR /service
#Start from clean repo
RUN rm -Rf /service/build || true
# Clean secret data & caches (Warning : deploying secret keys could be dangerous !)
RUN rm -f /service/data/\.*cache.json
ARG keepsecrets
RUN if [[ -z "$keepsecrets" ]]; then rm -f /service/data/.depositaddresses.json ;\
    rm -f /service/data/secret.json ;\
    else echo "Keeping secrets: warning do not deploy this docker image on a public space.";\
    fi

WORKDIR /service/build

ARG mode=Release
ARG test=0

RUN BUILD_MODE=${mode} && cmake -DCMAKE_BUILD_TYPE=${mode} -DCCT_ENABLE_TESTS=${test} -DCCT_ENABLE_ASAN=0 ..
RUN make -j 4

# Check if tests should be run: 
RUN if [[ "$test" == "1" ]]; then ctest; else echo "No tests."; fi

ENTRYPOINT [ "/service/build/coincenter" ]
