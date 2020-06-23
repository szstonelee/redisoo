package main

import (
	"context"
	"log"
	"net"

	pb "github.com/szstonelee/redisoo/go_sample_server/redisoo"

	"google.golang.org/grpc"
)

const (
	port = ":40051"
)

func reverse(in []byte) []byte {
	for i, j := 0, len(in)-1; i < j; i, j = i+1, j-1 {
		in[i], in[j] = in[j], in[i]
	}
	return in
}

type server struct {
	pb.UnimplementedRedisooServer
}

func (s *server) GetString(ctx context.Context, in *pb.GetStringRequest) (*pb.GetStringResponse, error) {
	log.Printf("Received: %v", in.GetKey())
	var r pb.GetStringResponse
	r.Status = pb.GetStringResponse_STATUS_REPLACE
	r.Value = reverse(in.Key)
	log.Printf("Reply status: %v, value: %v\n", r.Status, r.Value)
	return &r, nil
}

func main() {
	lis, err := net.Listen("tcp", port)
	if err != nil {
		log.Fatalf("failed to listen: %v", err)
	}
	s := grpc.NewServer()
	pb.RegisterRedisooServer(s, &server{})
	if err := s.Serve(lis); err != nil {
		log.Fatalf("failed to serve: %v", err)
	}
}
